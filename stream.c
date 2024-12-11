#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ftp.h"

ssize_t splice_stream( 
	stream_t stream_out,	off_t *out_offset,
	stream_t stream_in,     off_t *in_offset,    size_t count )
{
	ssize_t ret;
	void *buf;

	if( out_offset )
		lseek( stream_out.fd, *out_offset, SEEK_SET );
	
	if( in_offset )
		lseek( stream_in.fd ,  *in_offset, SEEK_SET );

	if( stream_in.type == S_FILE && stream_out.type == S_SOCKET )
		return sendfile( stream_out.fd, stream_in.fd, in_offset, 
				count );

	if( (buf = malloc( count )) == NULL )
	{
		errno = ENOMEM;
		return -1;
	}

	ret = read( stream_in.fd, buf, count );
	if( ret <= 0 )
	{
		free(buf);
		return ret;
	}

	if( in_offset ) *in_offset += ret;

	ret = write( stream_out.fd, buf, ret );

	if( out_offset ) *out_offset += ret;
	free(buf);
	return ret;
}
	
ssize_t sendall(int sockfd, const void *buf, size_t len, int flags)
{
	size_t total;
	ssize_t n;
	
	total = 0;
	while( total < len )
	{
		n = send( sockfd, 
			(const char *)buf + total, 
			len - total, 
			flags );
		if( n == -1 )
		{
			if( errno == EINTR )
				continue;
			else
				return -1;
		}
		
		total += n;
	}
	
	return total;
}


