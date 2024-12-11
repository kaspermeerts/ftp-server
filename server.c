#include <ctype.h> /* for toupper, tolower */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>  /* for inet_ntoa, etc.. */
#include <netinet/in.h>

#include "ftp.h"

static __malloc ftp_session_t *new_session(int sock, int write_pipe);
static void destroy_session(ftp_session_t *);
static ssize_t read_poll(int, char*, size_t);
static int read_cmd(ftp_command_t*, ftp_conn_t *);
static int remove_cmd( ftp_command_t * );
static int parse_cmd(ftp_command_t *);

int ftp_main( int sock, int write_pipe )
{
	int ret;
	ftp_session_t *session;
	ftp_conn_t *conn;
	ftp_command_t *command;

	if( init_reply_pool() )
		return 1;

	session = new_session(sock, write_pipe);
	if(session == NULL)
	{
		destroy_reply_pool();
		return 1;
	}

	conn = &session->conn;
	command = &session->command;

	log_info( "Client connected: %s\n", inet_ntoa(conn->client_addr) );
	
	reply( conn, "220 Welcome\r\n" );
	
	/* The main loop of our server. We read, parse, execute and remove
	 * commands in that order */
	while(1)
	{
		ret = read_cmd( command, conn);
		if(ret) break;
		
		ret = parse_cmd( command );
		if(ret) break;

		ret = do_cmd( session );
		if(ret) break;
			
		ret = remove_cmd( command );
		if(ret) break;
	}

	reply( conn, "421 Goodbye!\r\n" );
	
	log_info( "The connection to %s was closed.\n", 
				inet_ntoa(conn->client_addr) );

	destroy_state_pool();
	destroy_vfs_pool();
	destroy_reply_pool();
	destroy_session( session );
	return 1;
}

int server_handle_signal(void)
{
	sigset_t blockset, oldset;
	int ret;

	sigfillset( &blockset );
	sigprocmask( SIG_BLOCK, &blockset, &oldset );
	/* From here on, signals are blocked */

	ret = FTP_SUCCESS;
		
	/* This won't happen. */
	if( signal_flag & RECV_SIGCHLD )
		signal_flag &= ~RECV_SIGCHLD;		
	if( signal_flag & RECV_SIGPIPE )
		signal_flag &= ~RECV_SIGPIPE; /* Ignore */
	
	if( signal_flag ) /* All other signals means we terminate */
	{
		signal_flag = 0;
		ret = FTP_QUIT;
	}

	sigprocmask( SIG_SETMASK, &oldset, NULL );
	return ret;
	
}

static ftp_session_t *new_session(int sock, int write_pipe)
{
	ftp_session_t *session;
	ftp_conn_t conn;
	ftp_command_t command;
	ftp_login_t login;
	ftp_xfer_info_t info = {0};
	char *cmdline;
	struct sockaddr_in sa;
	socklen_t addrlen = sizeof( sa );

	session = malloc( sizeof *session );
	if( session == NULL )
	{
		FATAL_MEM(sizeof *session);
		return NULL;
	}

	/* Init communication */
	conn.master_pipe = write_pipe;
	conn.sock = sock;
	conn.pasv_sock = -1;
	conn.data_sock = -1;

	if( getpeername( sock, (struct sockaddr *) &sa, &addrlen ) == -1 )
	{
		log_fatal("Couldn't get client info: %m\n");
		free(session);
		return NULL;
	}
	conn.client_addr = sa.sin_addr;
	
	if( getsockname( sock, (struct sockaddr *) &sa, &addrlen ) == -1 )
	{
		log_fatal("Couldn't get socket name: %m\n");
		free(session);
		return NULL;
	}
	conn.host_addr = sa.sin_addr;
	
	session->conn = conn;

	/* Initialize command buffer */
	command.line_len = COMMAND_BUFFER_SIZE;
	cmdline = malloc( command.line_len );
	if(cmdline == NULL)
	{
		FATAL_MEM(command.line_len);
		free(session);
		return NULL;
	}	
	command.line = cmdline;
	command.dirty_len = 0;
	command.len = 0;
	command.arg = NULL;
	
	session->command = command;

	/* Init path structures */
	session->virt_path = malloc( FTP_MAX_PATH );
	if( session->virt_path == NULL )
	{
		FATAL_MEM( FTP_MAX_PATH );
		free(command.line);
		free(session);
		return NULL;
	}

	session->virt_path[0] = '/';
	session->virt_path[1] = '\0';

	session->filename = malloc( FTP_MAX_NAME );
	if( session->filename == NULL )
	{
		FATAL_MEM( FTP_MAX_NAME );
		free( command.line );
		free( session->virt_path );
		free( session );
		return NULL;
	}
	memset( session->filename, '\0', FTP_MAX_NAME );
	
	
	/* Session attributes */
	login.logged_in = false;
	login.anonymous = false;

	session->login = login;

	/* Transfer information */
	info.total_down = info.total_up = 0;
	info.xfer_len = 0;
	info.xfer_start = (struct timeval){0,0};
	info.xfer_status = FTP_SUCCESS;
	
	session->info = info;

	session->restart_pos = 0;
	
	return session;
}

static void destroy_session( ftp_session_t *session )
{
	if(session->conn.pasv_sock != -1)
		close(session->conn.pasv_sock);
	free(session->command.line);
	free(session->virt_path);
	free(session->filename);
	free(session->login.user);
	free(session);
	return;
}

/* Reads data from socket sock and stores it in buf 
 * Never reads more than max
 * Returns  	the amount of bytes read
 *		0 on when the connection is closed or the client timed out 
 *	   	-1 on failure
 */
static ssize_t read_poll( int sock, char *buf, size_t max )
{
	struct pollfd poll_fd;
	ssize_t ret;
	
	memset( &poll_fd, 0, sizeof( struct pollfd ) );
	poll_fd.fd	 = sock;
	poll_fd.events	|= POLLIN;
	
	/* Wait until there's incoming commands */
	ret = poll( &poll_fd, 1, config.idle_timeout );
	if( ret == 0 )
		return 0;
	else if(ret < 0 )
	{
		if(errno == EINTR && server_handle_signal() == FTP_QUIT)
			return 0;
		else
		{
			log_fatal("Unable to poll connection: %m\n");
			return -1;
		}
	}
	
	ret = recv( sock, buf, max, 0 );
	
	if(ret < 0)
	{
		if(errno == EINTR && server_handle_signal() == FTP_QUIT)
			return 0;
		else if(errno == ECONNRESET)
			return 0;
		else
		{
			log_fatal("recv() error: %m\n");
			return -1;
		}
	}
	
	return ret;
}

/* Reads from the control connection until we receive at least one command
 * Returns success, abort or error */
static int read_cmd( ftp_command_t *command, ftp_conn_t *conn )
{
	int len;
	
	/* Every commands ends with a NL, so we keep reading and filling
	 * the buffer until there is at least one NL in COMMAND->LINE */
	while(1)
	{
		if( memchr( command->line, '\n', command->dirty_len ) )
			return FTP_SUCCESS;
		
		
		len = read_poll(
			conn->sock, 
			command->line + command->dirty_len,
			command->line_len - command->dirty_len );
		
		if( len == 0 )
		{
			/* The remote side closed the connection
			 * or timed out */
			return FTP_QUIT;
		}
		else if ( len == -1 )
		{
			if( errno == EINTR )
			{ 
				if( server_handle_signal() == FTP_QUIT)
					return FTP_QUIT;
				else
					continue;
			}
			else
			{
				return FTP_ERROR;
			}
		}
		
		command->dirty_len += len;
			
		if( command->dirty_len >= command->line_len )
		{
			reply( conn, "421 Command line too long\r\n" );
			return FTP_QUIT;
		}
		
	}
	
	return FTP_SUCCESS;
}

/* Since we handled the last command, it's data is not dirty anymore.
 * We remove it from the commandline buffer
 * Always successful */
static int remove_cmd( ftp_command_t *command )
{
	
	if( command->len == command->dirty_len )
	{
		command->dirty_len = 0;
		return FTP_SUCCESS;
	}
	
	memmove( 
		command->line, 
		command->line     + command->len,
		command->line_len - command->len);
	
	command->dirty_len -= command->len;
	
	return FTP_SUCCESS;
}	

/* We know there is at least one command in the buffer
 * This will zero-terminate it and give us pointers to the command
 * and it's arguments 
 * Always successful */
 
static int parse_cmd( ftp_command_t *command )
{
	char *bufend;
	char *cmdend;
	char *argument;
	char *buf;

	buf = command->line;
	
	bufend = memchr( buf, '\n', command->line_len );
	
	if(!bufend) /* Shouldn't happen */
		return FTP_ERROR;
	
	/* Stringify it */
	*bufend = '\0';
	command->len = bufend - buf + 1;

	/* Command should end with \r\n but some clients just send \n */
	if(bufend != buf && *(bufend-1) == '\r' )
		*(bufend-1) = 0;
	
	cmdend = strchr( buf, ' ' );
	if( cmdend )
	{
		*(cmdend++) = 0;
		argument = cmdend;
	}
	else
		argument = bufend;
	
	/* RFC says we need to strip leading whitespace */
	while( *argument && isspace( *argument ) )
		argument++;
	
	command->arg = argument;

	if( strcasecmp( command->line, "PASS" ) != 0 )
		log_dbg( "<--- %s %s\n", command->line, command->arg );
	else
		log_dbg( "<--- PASS *******\n" );
	
	return FTP_SUCCESS;

}

/* Starts listening on an unpriviliged port
 * Returns the socket on success, -1 otherwise
 * Writes the port it is listening to in the parameter in big-endian*/
int pasv_listen( int pasv_sock, char port[2] )
{
	struct sockaddr_in sa;
	socklen_t socklen = sizeof sa;
	int listen_port;
	int start = config.pasv_port_start;
	int end = config.pasv_port_end + 1;
		
	{
	int yes = 1;
	if( setsockopt( pasv_sock, SOL_SOCKET, SO_REUSEADDR, 
			&yes, sizeof(yes) ) == -1 )
		log_warn("Unable to set socket options: %m");
		/* Non-fatal error */
	}

	for( listen_port = start; listen_port < end; listen_port++ )	
	{
		int ret;
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl( INADDR_ANY );
		sa.sin_port = htons( listen_port );
		
		ret = bind(pasv_sock, (struct sockaddr *) &sa, socklen);

		if( ret == -1 )
		{
			if( errno == EADDRINUSE )
				continue;
			
			log_warn("Unable to bind socket: %m");
			return FTP_ERROR;
		}
		else
			break;
	}
	
	if( listen_port == end )
	{
		log_warn("No free ports to listen to\n");
		return FTP_ERROR;
	}

	
	/* This will probably never fail */
	if(listen(pasv_sock, 1) == -1)
	{
		log_warn("listen() error: %m\n");
		return FTP_ERROR;
	}
	
	if(getsockname(pasv_sock, (struct sockaddr *)&sa, &socklen) == -1 )
	{
		log_warn("getsockname() error: %m");
		return FTP_ERROR;
	}

	/* Why don't we convert from network byte order?
	 * Because we know the network is big endian and 
	 * that's the way we'll print it out again. */
	memcpy(port, &sa.sin_port, 2);
	
	return FTP_SUCCESS;
}
