#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp.h"

static char *reply_pool = NULL;

int init_reply_pool(void)
{
	reply_pool = malloc( RESPONSE_BUFFER_SIZE );
	if( reply_pool == NULL )
	{
		FATAL_MEM( RESPONSE_BUFFER_SIZE );
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

int destroy_reply_pool(void)
{
	free(reply_pool);
	reply_pool = NULL;

	return FTP_SUCCESS;
}

int reply( ftp_conn_t *conn, const char *msg )
{
	/* This already ends with an \n */
	log_dbg( "---> %s", msg);
	return sendall( conn->sock, msg, strlen(msg), 0 );
}

int reply_format( ftp_conn_t *conn, const char *format, ... )
{
	va_list arg;
	int ret;

	assert( reply_pool != NULL );
	
	va_start( arg, format );
	  vsnprintf( reply_pool, RESPONSE_BUFFER_SIZE, format, arg );
	va_end( arg );
	
	ret = reply( conn, reply_pool );
	
	return ret;
}

