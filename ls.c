#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ftp.h"

typedef struct list_options
{
	bool opt_a;
/*	bool opt_l; */
} list_options_t;

static char *parse_list_options( char *, list_options_t * );
static int list_file( ftp_session_t *, char *);
static int list_directory( ftp_session_t *, char *, list_options_t *);

int dolist (ftp_session_t *session)
{
	char *argument;
	struct stat statarg;
	list_options_t ls_opts = {0};
	int ret;
	ftp_conn_t *conn = &session->conn;

	argument = session->command.arg;

	/* Parse arguments, eg:
	 * LIST -a -rd public */
	argument = parse_list_options( argument, &ls_opts );

	/* An empty argument means the current directory */
	if( *argument == '\0' )
		argument = ".";

	/* Check whether we have to list a directory or a file */
	if( vfs_stat( session->virt_path, argument, &statarg ) == -1 )
	{
		failed_vfs_reply( conn );
		return FTP_SUCCESS;
	}
	
	if( (conn->data_sock = accept_data_conn(conn)) == -1 )
		return FTP_SUCCESS;
	else if( conn->data_sock == -2 )
		return FTP_QUIT;

	reply( conn, "125 Data connection ok, transferring listing\r\n");

	if( S_ISDIR(statarg.st_mode) ) 
		ret = list_directory( session, argument, &ls_opts );
	else
		ret = list_file( session, argument );

	switch(ret)
	{
	case FTP_SUCCESS:
		reply( conn, "226 Listing sent OK\r\n");
		break;
	case FTP_ABOR:
		reply( conn, "226 Listing aborted\r\n");
		break;
	case FTP_FAIL:
		reply( conn, "550 Listing failed\r\n");
		break;
	case FTP_ERROR:
	default:
		reply(conn,"450 Error during write to data connection\r\n");
		break;
	}

	close(conn->data_sock);
	
	return FTP_SUCCESS;
}

static int send_filestat( const char *cwd, const char *vpath, int data_sock)
{
	static const char *months[] = 
		{ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	struct stat st;
	struct tm tm;
	const char *basename;
	char date[13];
	char statbuf[STAT_BUFFER_SIZE];
	int len;
	
	if( vfs_stat( cwd, vpath, &st ) == -1 )
		return FTP_FAIL;

	convert_time( &st.st_mtime, &tm );

	snprintf( date, sizeof(date), "%s %d %02d:%02d", months[tm.tm_mon],
			tm.tm_mday, tm.tm_hour, tm.tm_min );
			
	basename = strrchr( vpath, '/' );
	if( basename )
		basename++;
	else
		basename = vpath;
			
	/* Example: -rwxrwxrwx 1 1000 1000 4096 Jan 1 00:00 */
	len = snprintf( statbuf, STAT_BUFFER_SIZE,
		"%crwxrwxrwx %lu %lu %lu %lld %s %s\r\n",
		get_modechar( st.st_mode ),
		(unsigned long) st.st_nlink,
		(unsigned long) st.st_uid,
		(unsigned long) st.st_gid,
		(unsigned long long)	st.st_size,
		date,
		basename);
	
	if( sendall( data_sock, statbuf, len, 0  ) == -1 )
	{
		if( errno == EPIPE || errno == ECONNRESET )
			return FTP_ABOR;
		else
		{
			log_warn("Error sending file stats: %m\n");
			return FTP_ERROR;
		}
	}

	return FTP_SUCCESS;
}


static char *parse_list_options( char *arg, list_options_t *ls_opts )
{
	while( isblank( *arg ) )
		arg++;

	while( *arg == '-' )
	{
		arg++;
		while( *arg && !isblank( *arg ) )
		{
			switch( *arg )
			{
			case 'a':
				ls_opts->opt_a = true;
				break;
			default:
				break;
			}
			arg++;
		}

		while( isblank( *arg ) )
			arg++;
	}

	return trim_whitespace( arg );
}

static int list_file( ftp_session_t *session, char *filepath)
{
	int ret;

	ret = send_filestat( session->virt_path, filepath, session->conn.data_sock );

	return ret;
}

int list_directory( ftp_session_t *session, char *dirname, 
		list_options_t *ls_opts )
{
	DIR *drv;
	char temp_path_buffer[FTP_MAX_PATH];
	struct stat st;
	struct dirent *next;

	/* Save the current directory */
	strlcpy( temp_path_buffer, session->virt_path, FTP_MAX_PATH);

	vfs_chdir( session->virt_path, dirname );

	/* It's a directory, so open it and list the contents */
	if( (drv = vfs_opendir( session->virt_path, "." )) == NULL )
	{
		failed_vfs_reply(&session->conn);
		return FTP_ERROR;
	}
	
	while( (next = readdir( drv )) != NULL )
	{
		int ret;

		if( next->d_name[0] == '.' && !ls_opts->opt_a )
			continue;

		ret = send_filestat( session->virt_path, next->d_name, 
			session->conn.data_sock);

		/* We ignore failed stat's. */
		if( ret != FTP_SUCCESS && ret != FTP_FAIL )
			break;
	}

	vfs_closedir( drv );

	strlcpy( session->virt_path, temp_path_buffer, FTP_MAX_PATH );

	return FTP_SUCCESS;
}


