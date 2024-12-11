#include <assert.h>
#include <crypt.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "ftp.h"

static uid_t login_user( const char *user, const char *password );

int doacct( ftp_session_t *session )
{
	reply( &session->conn, "202 ACCT is obsolete\r\n" );
	return FTP_SUCCESS;
}

int douser ( ftp_session_t *session)
{
	char *username = session->command.arg;
	ftp_login_t *login = &session->login;
	ftp_conn_t *conn = &session->conn;

	if(login->logged_in)
	{
		reply(conn, "503 Already logged in\r\n");
		return FTP_SUCCESS;
	}

	session->login.user = NULL;

	if( strcasecmp( username, "anonymous" ) == 0 )
	{
		if( config.allow_anon == false )
		{
			reply(conn, 
				"550 No anonymous connections allowed\r\n");
			return FTP_SUCCESS;
		}

		reply(conn, "331 Any pass will do\r\n");
		login->anonymous = true;
		
	} else
	{
		session->login.user = strdup( username );
		if( !session->login.user )
		{
			FATAL_MEM( strlen( username ));
			return FTP_ERROR;
		}
		reply_format(conn, "331 Pass required for user %s\r\n", 
				session->login.user );
		return FTP_SUCCESS;
	}

	return FTP_SUCCESS;
}

int dopass( ftp_session_t *session )
{
	uid_t user_id;
	ftp_conn_t *conn = &session->conn;

	if( session->login.logged_in )
	{
		reply( conn, "503 Already logged in\r\n" );
		return FTP_SUCCESS;
	}

	if( session->login.anonymous )
	{
		reply(conn, "230 Hello!\r\n");
		init_vfs_pool( config.anon_root_dir );
		init_state_pool();
		session->login.logged_in = true;
		return FTP_SUCCESS;
	}

	if( session->login.user == NULL )
	{
		reply( conn, "503 Login with USER first\r\n" );
		return FTP_SUCCESS;
	}

	user_id = login_user( session->login.user, session->command.arg );
	if( user_id == -1 )
	{
		reply( conn, "530 Login failed\r\n" );
		return FTP_QUIT;
	}

	if( setuid( user_id ) == -1 )
	{
		log_fatal( "Couldn't set user id: %m\n");
		return FTP_ERROR;
	}

	send_state( session, T_LOGIN );

	session->login.logged_in = true;
	reply( conn, "230 Login successful, have fun\r\n" );
	return FTP_SUCCESS;
}

static uid_t login_user( const char *user, const char *password )
{
	int ret;
	char buffer[PASSWORD_BUFFER_SIZE];
	struct spwd *spwd;
	struct passwd pwd, *res;
	struct crypt_data crypt_buffer;
	char *encrypt_pw;

	ret = getpwnam_r( user, &pwd, buffer, sizeof(buffer), &res);
	if( res == NULL )
	{
		if( ret != 0 )
		{
			errno = ret;
			log_warn("Password database error: %m\n");
		}
		return -1;
	}

	spwd = getspnam( user );
	if( spwd == NULL )
	{
		log_warn( "Unable to read shadow database: %m\n" );
		return -1;
	}

	crypt_buffer.initialized = 0;
	encrypt_pw = crypt_r( password, spwd->sp_pwdp, &crypt_buffer );
	
	if( encrypt_pw == NULL )
	{
		log_warn( "Unable to encrypt password: %m\n");
		return -1;
	}

	if( strcmp( encrypt_pw, spwd->sp_pwdp ) != 0 )
	{
		log_warn("Warning, wrong password entered for user %s\n",
				user );
		return -1;
	}

	if( init_state_pool() )
		return -1;

	if( init_vfs_pool( pwd.pw_dir ) )
		return -1;

	return pwd.pw_uid;

}

