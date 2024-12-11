#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "ftp.h"

static int transfer_fd_to_net ( ftp_session_t *, int, off_t, off_t );

static cmd_handler_t core_command_list[] = {
	/* NAME function needs_login, needs_data, needs_arg */
	{ "ABOR", &doabor, true,  false, false },
	{ "ACCT", &doacct, true,  false, false },
	{ "ALLO", &doallo, true,  false, false },
	{ "CDUP", &docdup, true,  false, false },
	{ "CLNT", &doclnt, true,  false, false },
	{ "CWD" , &docwd,  true,  false, true  },
	{ "FEAT", &dofeat, false, false, false },
	{ "LIST", &dolist, true,  true , false },
	{ "MDTM", &domdtm, true,  false, true  },
	{ "MKD",  &domkd,  true,  false, true  },
	{ "NOOP", &donoop, false, false, false },
	{ "OPTS", &doopts, false, false, false },
	{ "PASS", &dopass, false, false, false },
	{ "PASV", &dopasv, true,  false, false },
	{ "PWD" , &dopwd,  true,  false, false },
	{ "QUIT", &doquit, false, false, false },
	{ "REST", &dorest, true,  false, true  },
	{ "DELE", &dodele, true,  false, true  },
	{ "RETR", &doretr, true,  true , true  },
	{ "RMD",  &dormd,  true,  false, true  },
	{ "SIZE", &dosize, true,  false, true  },
	{ "SYST", &dosyst, false, false, false },
	{ "TYPE", &dotype, true,  false, true  },
	{ "USER", &douser, false, false, false },
	{ "STOR", &dostor, true,  true,  true  },
	{ 0 },
	};


int init_core_commands(void)
{
	log_dbg("Initializing core command list\n");

	return add_command_list( core_command_list );
}

int dosyst (ftp_session_t *session)
{
	reply( &session->conn, "200 UNIX Type L8\r\n" );
	return FTP_SUCCESS;
}

int donoop (ftp_session_t *session)
{
	reply( &session->conn, "200 Zzz\r\n");
	return FTP_SUCCESS;
}

int dotype (ftp_session_t *session)
{
	/* This is all pretty useless, we're not in the 80's anymore */
	char c;
	ftp_conn_t *conn = &session->conn;
	c = session->command.arg[0];

	switch( toupper(c) )
	{
	case 'A':
		reply(conn, "200 TYPE is now ASCII\r\n");
		break;
	case 'I':
		reply(conn, "200 TYPE is now BINARY\r\n");
		break;
	case 'L':
		if( session->command.arg[1] )
			reply(conn, "501 Missing argument\r\n");
		else
		{
			reply(conn, "504-Only 8-bit bytes allowed\r\n");
			reply(conn, "504 TYPE is now Binary\r\n");

		}
		break;
	default:
		reply(conn, "504 Bad parameter\r\n");
		break;
	}	

	return FTP_SUCCESS;
}

		
int dopwd (ftp_session_t *session)
{
	ftp_conn_t *conn = &session->conn;
	
	/* We oughta double up the quotes. We don't */
	reply_format(conn, "257 \"%s\" is your current location\r\n", 
			session->virt_path);
	
	return FTP_SUCCESS;
}

int docwd (ftp_session_t *session)
{
	struct stat statpath;
	ftp_conn_t *conn = &session->conn;

	if( vfs_stat( session->virt_path, session->command.arg, &statpath )
			== -1 )
		return failed_vfs_reply( conn );

	if( !S_ISDIR( statpath.st_mode ))
	{
		reply(conn, "550 Not a directory\r\n");
		return FTP_SUCCESS;
	}
	
	vfs_chdir( session->virt_path, session->command.arg );

	send_state( session, T_CHDIR );

	reply( conn, "250 OK\r\n" );
	
	return FTP_SUCCESS;
}

int docdup (ftp_session_t *session)
{
	char parent[] = "..";
	session->command.arg = parent;
	return docwd( session );
}

int dopasv (ftp_session_t *session)
{
	char ip[4], port[2];
	int pasv_sock;
	ftp_conn_t *conn;
	conn = &session->conn;
	
	if(conn->pasv_sock != -1)
	{
		close(conn->pasv_sock);
		conn->pasv_sock = -1;
	}
	
	/* param now contains our own IP in big endian */
	memcpy( ip, &conn->host_addr.s_addr, 4 );

	pasv_sock = socket( AF_INET, SOCK_STREAM, 0 );

	if( pasv_sock == -1 )
	{
		log_warn("Unable to open listening socket: %m\n");
		reply(conn, "425 Can't open data connection\r\n");
		return -1;
	}

	conn->pasv_sock = pasv_sock;

	if( pasv_listen(pasv_sock, port) == FTP_ERROR )
	{
		close( pasv_sock );
		conn->pasv_sock = -1;
		reply(conn, "425 Can't open data connection\r\n");
		return FTP_SUCCESS;
	}

	reply_format(conn, 
		"227 Entering Passive Mode "
		"(%hhu,%hhu,%hhu,%hhu,%hhu,%hhu)\r\n", 
		ip[0], ip[1], ip[2], ip[3], 
		port[0], port[1] );
		

	return FTP_SUCCESS;
}

int dosize (ftp_session_t *session)
{
	struct stat st;
	unsigned long long size;
	ftp_conn_t *conn;
	conn = &session->conn;
	
	if(vfs_stat( session->virt_path, session->command.arg, &st ) == -1)
	{
		failed_vfs_reply(conn);
		return FTP_SUCCESS;
	}

	if(!S_ISREG(st.st_mode))
	{
		reply(conn, "550 Can only size regular files\r\n");
		return FTP_SUCCESS;
	}

	size = (unsigned long long) st.st_size;
	reply_format(conn, "213 %llu\r\n", size);
	return FTP_SUCCESS;
}

int domdtm (ftp_session_t *session)
{
	struct stat st;
	struct tm tm;
	char date[15];
	ftp_conn_t *conn;
	conn = &session->conn;
	
	if( vfs_stat( session->virt_path, session->command.arg, &st ) == -1 )
	{
		failed_vfs_reply(conn);
		return FTP_SUCCESS;
	}
	
	if(!S_ISREG(st.st_mode))
	{
		reply(conn, "550 Can only time regular files\r\n");
		return FTP_SUCCESS;
	}
	
	convert_time( &st.st_mtime, &tm );

	/* This will need exactly 14 bytes and a nul byte */
	snprintf(date, 15, "%04d%02d%02d%02d%02d%02d", 
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday, 
		tm.tm_hour, tm.tm_min, tm.tm_sec );
	
	reply_format(conn, "213 %s\r\n", date );
	
	return FTP_SUCCESS;
}

int doquit (ftp_session_t *session)
{
	reply(&session->conn, "221 Goodbye!\r\n");
	return FTP_QUIT;
}


int dofeat (ftp_session_t *session)
{
	ftp_conn_t *conn = &session->conn;

	reply(conn, "211-Extensions supported:\r\n");
	reply(conn, " SIZE\r\n");
	reply(conn, " MDTM\r\n");
	reply(conn, "211 End.\r\n");

	return FTP_SUCCESS;
}

int doopts (ftp_session_t *session)
{
	reply( &session->conn, "501 No such command\r\n");
	return FTP_SUCCESS;
}

int doabor( ftp_session_t *session )
{
	reply( &session->conn, "226 Abort successful\r\n");
	return FTP_SUCCESS;
}

int dorest( ftp_session_t *session )
{
	long long offset;
	char *strpos = trim_whitespace( session->command.arg );

	offset = strtoll( strpos, NULL, 10 );
	
	if( offset < 0 || offset == LLONG_MAX || errno == ERANGE )
	{
		reply( &session->conn, "501-Invalid seek argument\r\n" );
		reply( &session->conn, "501 Seek reset to 0\r\n");
		offset = 0;
	} else
		reply_format( &session->conn,
			"350 Restarting transfer at %llu\r\n", offset );

	session->restart_pos = (off_t) offset;

	return FTP_SUCCESS;
}

static int transfer_file
	( ftp_session_t *session, stream_t data, stream_t file, 
	  off_t file_offset, off_t count)
{
	int blocks, i, ret;
	off_t remainder, remainder_offset;

	gettimeofday( &session->info.xfer_start, NULL );
	session->info.xfer_probe = session->info.xfer_start;
	session->info.xfer_len = session->info.probe_len = 0;
	blocks =    count / XFER_BLOCK_SIZE;
	remainder = count % XFER_BLOCK_SIZE;

	/* Send blocks of data and throttle the connection.
	 * This is a busy loop, so handle signals */
	for( i = 0; i < blocks; i++ )
	{
		off_t block_offset = file_offset + i * XFER_BLOCK_SIZE;

		while( (ret = 
			splice_stream( data, NULL, file, &block_offset, 
				XFER_BLOCK_SIZE )) == -1 )
		{
			switch( errno )
			{
			case EINTR:
				if( server_handle_signal() )
					return FTP_QUIT;
				else
					continue;
				break;
			case EPIPE:
			case ECONNRESET:
				return FTP_ABOR;
			default:
				log_fatal("Splice error: %m\n");
				return FTP_ERROR;
			}
		}

		if( ret == 0 )
			return FTP_ABOR;

		session->info.xfer_len  += XFER_BLOCK_SIZE;
		session->info.probe_len += XFER_BLOCK_SIZE;

		throttle_pause( session );

		if( server_handle_signal() )
			return FTP_QUIT;
	}
	
	/* Send the remaining data < XFER_BLOCK_SIZE */

	remainder_offset = file_offset + count - remainder;

	while( splice_stream( data, NULL, file, &remainder_offset, 
			remainder ) == -1 )
	{
		switch( errno )
		{
		case EINTR:
			if( server_handle_signal() )
				return FTP_QUIT;
			else
				continue;
			break;
		case EPIPE:
		case ECONNRESET:
			return FTP_ABOR;
		default:
			log_fatal("Splice error: %m\n");
			return FTP_ERROR;
		}
	}

	session->info.xfer_len += remainder;

	return FTP_SUCCESS;
}

int store_file( ftp_session_t *session, stream_t file, stream_t data )
{
	int ret;
	off_t offset = session->restart_pos;

	gettimeofday( &session->info.xfer_start, NULL );
	session->info.xfer_probe = session->info.xfer_start;
	session->info.xfer_len = session->info.probe_len = 0;

	while(ret = splice_stream( file, &offset, data, 0, XFER_BLOCK_SIZE))
	{
		if( ret == -1 )
		{
			switch(errno)
			{
			case EINTR:
				if( server_handle_signal() )
					return FTP_QUIT;
				else
					continue;
				break;
			case EPIPE:
			case ECONNRESET:
				return FTP_ABOR;
			default:
				log_fatal("Splice error: %m\n");
				return FTP_ERROR;
			}
		}
		
		session->info.probe_len += ret;
		session->info.xfer_len  += ret;

		throttle_pause( session );

		if( signal_flag )
			if( server_handle_signal() )
				return FTP_QUIT;

	}

	return FTP_SUCCESS;

}

int doretr (ftp_session_t *session)
{
	const char *basename;
	const char *argument = session->command.arg;
	struct stat statfile;
	off_t filesize, offset;
	ftp_conn_t *conn = &session->conn;
	int ret, fd;
	stream_t file, data;

	basename = find_basename( argument );

	if( strlcpy( session->filename, basename, FTP_MAX_NAME - 1 ) 
			>= FTP_MAX_NAME )
	{
		reply(conn, "550 Filename too long\r\n");
		return FTP_SUCCESS;
	}
		
	if( vfs_stat( session->virt_path, argument, &statfile ) == -1 )
	{
		failed_vfs_reply( conn );
		return FTP_SUCCESS;
	}
	
	if( !S_ISREG( statfile.st_mode ) )
	{
		reply( conn, "550 Can only retrieve regular files\r\n" );
		return FTP_SUCCESS;
	}

	fd = vfs_open( session->virt_path, argument, O_RDONLY );
	if( fd == -1 )
	{
		failed_vfs_reply( conn );
		return FTP_SUCCESS;
	}
	file.fd = fd;
	file.type = S_FILE;

	conn->data_sock = accept_data_conn( conn );
	if( conn->data_sock == -1 )
	{
		vfs_close(fd);
		return FTP_SUCCESS;
	}
	else if( conn->data_sock == -2 )
	{
		vfs_close(fd);
		return FTP_QUIT;
	}

	data.fd = conn->data_sock;
	data.type = S_SOCKET;

	offset = session->restart_pos;
	filesize = statfile.st_size;

	if( offset > filesize )
	{
		vfs_close( fd );
		close( conn->data_sock );
		reply_format(conn,
			"451 Restarting position %llu too "
			"large for file %s of size %llu\r\n",
			(unsigned long long) offset,
			basename,
			(unsigned long long) filesize );
		return FTP_SUCCESS;
	}

	reply(conn, "125 Data connection OK, transfer starting\r\n");
	session->info.xfer_status = 0;

	send_state( session, T_XFER_START );

	ret = transfer_file(session, data, file, offset, filesize - offset);

	if( ret == FTP_SUCCESS )
		reply(conn, "226 File transfer successful\r\n");
	else if ( ret == FTP_ABOR )
		reply(conn, "426 File transfer aborted\r\n");
	else
		reply(conn,"450 Error during write to data connection\r\n");

	session->info.total_down += session->info.xfer_len;
	session->restart_pos = 0;
	session->info.xfer_status = ret;
	
	send_state( session, T_XFER_STOP );

	vfs_close(fd);
	close(conn->data_sock);

	return FTP_SUCCESS;
}

int dostor( ftp_session_t *session )
{
	ftp_conn_t *conn = &session->conn;
	const char *pathname = session->command.arg;
	const char *basename;
	int fd, ret;
	stream_t file, data;

	basename = find_basename( pathname );

	if( strlcpy( session->filename, basename, FTP_MAX_NAME - 1 ) 
			>= FTP_MAX_NAME )
	{
		reply(conn, "550 Filename too long\r\n");
		return FTP_SUCCESS;
	}

	fd = vfs_creat( session->virt_path, pathname, 0777 );
	if( fd  == -1 )
	{
		failed_vfs_reply( conn );
		return FTP_SUCCESS;
	}
	file.fd = fd;
	file.type = S_FILE;

	fd = accept_data_conn( conn );
	if( fd == -1 )
	{
		vfs_close( fd );
		return FTP_SUCCESS;
	}
	else if( fd == -2 )
	{
		vfs_close( fd );
		return FTP_QUIT;
	}
	data.fd = fd;
	data.type = S_SOCKET;

	reply(conn, "125 Data connection OK, transfer starting\r\n");
	session->info.xfer_status = 0;

	send_state( session, T_XFER_START );

	ret = store_file( session, file, data );

	if( ret == FTP_SUCCESS )
		reply(conn, "226 File transfer successful\r\n");
	else if ( ret == FTP_ABOR )
		reply(conn, "426 File transfer aborted\r\n");
	else
		reply(conn,"450 Error during write to data connection\r\n");

	session->info.total_up += session->info.xfer_len;
	session->info.xfer_status = ret;

	send_state( session, T_XFER_STOP );

	vfs_close( file.fd );
	close( data.fd );

	return FTP_SUCCESS;
}


int doclnt( ftp_session_t *session )
{
	reply(&session->conn, "220 Whatever\r\n");

	return FTP_SUCCESS;
}

int domkd( ftp_session_t *session )
{
	if( vfs_mkdir( session->virt_path, session->command.arg, 0777 ) )
	{
		failed_vfs_reply( &session->conn );
		return FTP_SUCCESS;
	}
	
	reply_format( &session->conn, "257 \"%s\" created\r\n", 
			session->command.arg );


	return FTP_SUCCESS;
}


int dormd( ftp_session_t *session )
{
	if( vfs_rmdir( session->virt_path, session->command.arg ) == -1 )
	{
		failed_vfs_reply( &session->conn );
		return FTP_SUCCESS;
	}

	reply( &session->conn, "250 Directory removed\r\n" );

	return FTP_SUCCESS;
}

int doallo( ftp_session_t *session )
{
	reply( &session->conn, "202 ALLO is obsolete\r\n" );
	
	return FTP_SUCCESS;
}

int dodele( ftp_session_t *session )
{
	int ret;
	const char *path = session->command.arg;

	ret = vfs_unlink( session->virt_path, path );
	if( ret == -1 )
	{
		failed_vfs_reply( &session->conn );
		return FTP_SUCCESS;
	}

	reply( &session->conn, "250 File deleted\r\n" );

	return FTP_SUCCESS;
}
