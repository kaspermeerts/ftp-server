#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ftp.h"

static bool is_leap( int year );

static bool is_leap( int year )
{
	if( year % 400 )
		return true;
	else if( year % 100 )
		return false;
	else if( year % 4 )
		return true;
}

struct tm *convert_time( time_t *timep, struct tm *dest )
{
	static const int days_in_month[] = 
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	time_t time;
	int days, month, year;

	time = *timep;

	dest->tm_hour = ( time / 3600 ) % 24;
	dest->tm_min  = ( time / 60 ) % 60;
	dest->tm_sec  = time % 60;

	/* 86400 seconds in a day */
	days = time / 86400;
	/* January 1, 1970 was a thursday */
	dest->tm_wday = (days + 4) % 7;

	year = 1970;

	while( days >= ( is_leap( year ) ? 366 : 365 ) )
	{
		days -= is_leap( year ) ? 366 : 365;
		year++;
	}

	dest->tm_year = year - 1900;
	dest->tm_yday = days;

	month = 0;
	while( 1 )
	{
		int i = days_in_month[month];
		if( month == 1 && is_leap(year) )
			i++; /* Februari of course sometimes has 29 days */
		if( days < i )
			break;
		days -= i;
		month++;
	}

	dest->tm_mon  = month;
	dest->tm_mday = days + 1;
	dest->tm_isdst = 0; /* I hate DST */

	return dest;
}

/* strcpy and strncpy are both extremely unsafe and strncpy also has an
 * abysmally low performance. This implementation of strlcpy is faster than
 * Glibc's strcpy */
size_t strlcpy( char *dst, const char *src, size_t size )
{
	const size_t len = strlen( src );

	if( len )
	{
		size_t tlen = ( len > size - 1 ) ? size - 1 : len;
		memcpy( dst, src, tlen );
		dst[tlen] = '\0';
	}

	return len;
}

char *trim_whitespace( char *line )
{
	char *end;
	size_t len;

	/* Skip leading whitespace */
	while( isblank( *line ) )
		line++;

	/* Trim trailing whitespace */
	len = strlen( line );
	if( len == 0 )
		return line;
	end = line + len - 1;
	while( isspace(*end) )
		end--;
	*(end+1) = '\0';

	return line;
}

int seek_pipe( int pipefd, size_t offset )
{
	char buffer[SEEK_BUFFER_SIZE];
	int blocks, i;
	size_t remainder;
	
	if( offset == 0 )
		return FTP_SUCCESS;
	
	blocks = offset / SEEK_BUFFER_SIZE;
	remainder = offset % SEEK_BUFFER_SIZE;

	for( i = 0; i < blocks; i++ )
	{
		if( read( pipefd, buffer, SEEK_BUFFER_SIZE ) == -1 )
		{
			log_fatal("Reading from pipe failed: %m\n");
			return FTP_ERROR;
		}
	}

	if( read( pipefd, buffer, remainder ) == -1 )
	{
		log_fatal("Reading from pipe failed: %m\n");
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

int msecdiff( struct timeval *t1, struct timeval *t2 )
{
	return	(t1->tv_sec - t2->tv_sec ) * 1000 +
		(t1->tv_usec - t2->tv_usec ) / 1000;
}

char get_modechar( mode_t mode )
{
	if( S_ISDIR( mode ) )
		return 'd';
	else if( S_ISREG( mode ) )
		return '-';
	else if( S_ISCHR( mode ) )
		return 'c';
	else if( S_ISBLK( mode ) )
		return 'b';
	else if( S_ISFIFO( mode ) )
		return 'p';
	else if( S_ISLNK( mode ) )
		return 'l';
	else if( S_ISSOCK( mode ) )
		return 's';
	else
		return '-';
}

int accept_data_conn( ftp_conn_t *conn )
{
	int data_sock;

	while( ( data_sock = accept( conn->pasv_sock, NULL, NULL )) == -1 )
	{
		if( errno == EINTR && server_handle_signal() )
			return -2;
		else
		{
			if( errno == ECONNABORTED )
				return -2;
			log_warn("Unable to accept data connection: %m\n");
			reply(conn, "425 Cannot open data connection\r\n");
			return -1;
		}
	}

	return data_sock;
}
				
const char *find_basename( const char *path )
{
	const char *sep;
	sep = strrchr( path, '/' );
	return sep ? sep + 1 : path;
}


