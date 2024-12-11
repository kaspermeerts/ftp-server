#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/time.h>

#include "ftp.h"

static int recv_login( int read_pipe, ftp_child_t *child );
static int recv_chdir( int read_pipe, ftp_child_t *child );
static int recv_xfer( int read_pipe, ftp_child_t *child );
static int recv_xfer_start( int read_pipe, ftp_child_t *child );
static int recv_xfer_stop( int read_pipe, ftp_child_t *child );

static int send_login( ftp_session_t *session, void *buf );
static int send_chdir( ftp_session_t *session, void *buf );
static int send_xfer( ftp_session_t *session, void *buf );
static int send_xfer_start( ftp_session_t *session, void *buf );
static int send_xfer_stop( ftp_session_t *session, void *buf );

typedef struct
{
	int type;
	int (*recv_fun)(int, ftp_child_t *);
	int (*send_fun)(ftp_session_t *, void *);
	size_t buf_size;
} state_ops_t;

static const state_ops_t state_ops[] = {
{ T_LOGIN,	&recv_login, 	  &send_login,      64 },
{ T_CHDIR,	&recv_chdir,	  &send_chdir,	    FTP_MAX_PATH },
{ T_XFER_START,	&recv_xfer_start, &send_xfer_start, FTP_MAX_NAME },
{ T_XFER,	&recv_xfer, 	  &send_xfer,	   sizeof(ftp_xfer_info_t)},
{ T_XFER_STOP,	&recv_xfer_stop,  &send_xfer_stop, sizeof(ftp_xfer_info_t)}
};

static void *state_pool = NULL;

#define NUM_STATE_OPS (sizeof(state_ops)/sizeof(*state_ops))

int init_state_pool(void)
{
	unsigned int i;
	size_t max = 0;

	for( i = 0; i < NUM_STATE_OPS; i++ )
		if( state_ops[i].buf_size > max )
			max = state_ops[i].buf_size;
	
	state_pool = malloc( max );
	if( state_pool == NULL )
	{
		FATAL_MEM( max );
		return FTP_ERROR;
	}

	memset( state_pool, '\0', max );

	return FTP_SUCCESS;
}

int destroy_state_pool(void)
{
	free(state_pool);
	state_pool = NULL;
	return FTP_SUCCESS;
}
		

int recv_state( int read_pipe, ftp_child_t *head )
{
	ftp_state_t state;
	ftp_child_t *child;
	int ret;
	unsigned int i;

	if( read( read_pipe, &state, sizeof state ) == -1 )
	{
		log_fatal("Couldn't receive state: %m\n");
		return FTP_ERROR;
	}

	if( state.magic != STATE_MAGIC )
	{
		log_fatal("Received state with incorrect magic 0x%x\n",
				state.magic );
		log_fatal("Magic should be 0x%x\n", STATE_MAGIC );
		return FTP_ERROR;
	}

	for( i = 0; i < NUM_STATE_OPS; i++ )
		if( state_ops[i].type == state.type)
			break;

	if( i == NUM_STATE_OPS )
	{
		log_fatal("Received message of unknown type %d\n", 
				state.type );
		return FTP_ERROR;
	}

	child = find_client( head, state.pid );
	if( child == NULL )
	{
		/* We received a message from an unknown client.
		 * This is not neccessarily a bug, sometimes messages
		 * arrive after the child has died. We could avoid this
		 * with some sort of acknowledgment system, but that would
		 * be detrimental to performance and offer no real benefit.
		 */
		return seek_pipe( read_pipe, state_ops[i].buf_size );
	}
	
	ret = state_ops[i].recv_fun( read_pipe, child );
	
	return ret;
}

static int recv_login( int read_pipe, ftp_child_t *child )
{
	if( read( read_pipe, child->login_name, MAX_LOGIN_NAME ) == -1 )
	{
		log_fatal("Coudln't read login name: %m\n");
		return FTP_ERROR;
	}

	log_info("New login: %s\n", child->login_name );

	return FTP_SUCCESS;
}

static int recv_chdir( int read_pipe, ftp_child_t *child )
{
	while( read( read_pipe, child->path, FTP_MAX_PATH ) == -1 )
	{
		log_fatal("Coudln't read directory: %m\n");
		return FTP_ERROR;
	}

	log_info("Client changed directory to %s\n", child->path );

	return FTP_SUCCESS;
}

static int recv_xfer( int read_pipe, ftp_child_t *child )
{
	if( read( read_pipe, &child->xfer_info, sizeof( ftp_xfer_info_t )) 
			== -1 )
	{
		log_fatal("Couldn't receive transfer info: %m\n");
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

static int recv_xfer_start( int read_pipe, ftp_child_t *child )
{
	if( read( read_pipe, child->filename, FTP_MAX_NAME ) == -1 )
	{
		log_fatal("Couldn't receive transfer info: %m\n");
		return FTP_ERROR;
	}
	child->filename[FTP_MAX_NAME-1] = '\0';
	child->xfer_in_progress = true;

	log_info("Download of %s started\n", child->filename );

	return FTP_SUCCESS;

}

static int recv_xfer_stop( int read_pipe, ftp_child_t *child )
{
	(void) read_pipe;
	struct timeval tv;
	const char *stat_string;
	int diff, rate;
	
	if(read(read_pipe, &child->xfer_info, sizeof(ftp_xfer_info_t))== -1)
	{
		log_fatal("Coudln't receive transfer info: %m\n");
		return FTP_ERROR;
	}

	switch( child->xfer_info.xfer_status )
	{
	case FTP_SUCCESS:
		stat_string = "completed";
		break;
	case FTP_ABOR:
		stat_string = "aborted";
		break;
	case FTP_ERROR:
	case FTP_QUIT:
	default:
		stat_string = "failed";
		break;
	}


	child->xfer_in_progress = false;

	/* Calculate the rate at which this download went */
	gettimeofday( &tv, NULL );
	diff = msecdiff( &tv, &child->xfer_info.xfer_start ); 

	/* Very, very fast transfers could complete 
	 * in less than a millisecond */
	if( diff == 0 ) 
		diff = 1;
	rate = child->xfer_info.xfer_len / diff;

	log_info( "Transfer of %s %s (%d kB/s)\n", 
			child->filename, stat_string, rate );

	return FTP_SUCCESS;
}

int send_state( ftp_session_t *session, int type )
{
	ftp_state_t new_state;
	unsigned int i;
	int ret;
	struct iovec iov[2];
	ftp_conn_t *conn;
	conn = &session->conn;

	assert( state_pool != NULL );

	for( i = 0; i < NUM_STATE_OPS; i++ )
		if( state_ops[i].type == type )
			break;
	
	if( i == NUM_STATE_OPS )
	{
		log_fatal("Couldn't send message with unknown type %d\n", 
				type );
		return FTP_ERROR;
	}

	ret = state_ops[i].send_fun( session, state_pool );
	if(ret != FTP_SUCCESS )
		return ret;
	
	new_state.pid = getpid();
	new_state.magic = STATE_MAGIC;
	new_state.type = type;

	iov[0].iov_base = &new_state;
	iov[0].iov_len  = sizeof(new_state);
	iov[1].iov_base = state_pool;
	iov[1].iov_len  = state_ops[i].buf_size;

	while( writev( conn->master_pipe, iov, 2 ) == -1 )
	{
		if( errno == EINTR )
			continue;
		log_fatal("Couldn't send state: %m\n");
		return FTP_ERROR;
	}

	return FTP_SUCCESS;

}

static int send_login( ftp_session_t *session, void *buf )
{
	ftp_login_t *login;
	login = &session->login;

	memset( buf, '\0', MAX_LOGIN_NAME );

	if( login->anonymous )
		strlcpy( buf, "anonymous", MAX_LOGIN_NAME );
	else
		strlcpy( buf, session->login.user, MAX_LOGIN_NAME );

	return FTP_SUCCESS;
}

static int send_chdir( ftp_session_t *session, void *buf )
{
	size_t len;
	
	len = strlcpy( buf, session->virt_path, FTP_MAX_PATH );

	if( len >= FTP_MAX_PATH )
	{
		log_warn("Path too long: %s\n", session->virt_path);
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

static int send_xfer( ftp_session_t *session, void *buf )
{
	*(ftp_xfer_info_t *)buf = session->info;

	return FTP_SUCCESS;

}

static int send_xfer_start( ftp_session_t *session, void *buf )
{
	const char *filename = session->filename;

	if(strlcpy( buf, filename, FTP_MAX_NAME ) > FTP_MAX_NAME )
	{
		log_fatal("Filename too long: '%s'\n", filename);
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

static int send_xfer_stop( ftp_session_t *session, void *buf )
{
	*(ftp_xfer_info_t *)buf = session->info;

	return FTP_SUCCESS;

}
