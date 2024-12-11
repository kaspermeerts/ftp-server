#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include "ftp.h"

static bool syslog_enabled = false;

static int vlogger( const int , const char *, va_list );

int start_log(void)
{
	/* Yay, we got our own facility :) */
	if( config.syslog )
	{
		openlog( PROGNAME, LOG_NDELAY | LOG_PID, LOG_FTP );
		syslog_enabled = true;
	}
	log_dbg("Starting log\n");
	return FTP_SUCCESS;
}

static int vlogger( const int urgency, const char *format, va_list args )
{
	if( config.syslog && syslog_enabled )
	{
		va_list args2;
		va_copy( args2, args );
		vsyslog( urgency, format, args2 );
		va_end( args2 );
	}

	fprintf(  stderr, "%s[%d]: ", PROGNAME, getpid() );
	vfprintf( stderr, format, args );

	return 0;
}

int logger( const int urgency, const char *format, ...)
{
	va_list args;
	
	va_start( args, format );
	 vlogger( urgency, format, args );
	va_end( args );
	
	return 0;
}
				
int log_fatal( const char *format, ... )
{
	va_list args;
	
	va_start( args, format );
	 vlogger( LOG_ERR, format, args );
	va_end( args );
	
	return 0;
}

int log_warn( const char *format, ... )
{
	va_list args;
	
	va_start( args, format );
	 vlogger( LOG_WARNING, format, args );
	va_end( args );
	
	return 0;
}

int log_info( const char *format, ... )
{
	va_list args;
	
	va_start( args, format );
	 vlogger( LOG_INFO, format, args );
	va_end( args );
	
	return 0;
}

int log_dbg( const char *format, ... )
{
	if( !config.debug )
		return 1;

	va_list args;

	va_start( args, format );
	 vlogger( LOG_DEBUG, format, args );
	va_end( args );

	return 0;
}
