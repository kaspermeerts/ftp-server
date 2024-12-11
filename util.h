#ifndef __FTPUTIL_H__
#define __FTPUTIL_H__ 1
#include "server.h"

extern struct tm *convert_time( time_t *timep, struct tm *dest );
extern size_t strlcpy( char *, const char *, size_t );
extern __must_check char *trim_whitespace(char *);
extern int seek_pipe( int pipefd, size_t offset );
extern int msecdiff( struct timeval *t1, struct timeval *t2 );
extern char get_modechar( mode_t mode );
extern int accept_data_conn( ftp_conn_t * );
extern const char *find_basename( const char *path );

#define FATAL_MEM(n)	(log_fatal("No memory for %ld bytes\n", (long) (n)))

#define RESPONSE_BUFFER_SIZE	1024
#define STAT_BUFFER_SIZE	( 128 + FTP_MAX_NAME ) 
				/* 128 bytes for the stat data
				 * -rwxrwxrwx etc... */
#define SEEK_BUFFER_SIZE	1024

#endif /* __FTPUTIL_H__ */

