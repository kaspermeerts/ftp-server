#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ftp.h"

static ssize_t vfs_realpath( const char *, const char *);
static char *real_path = NULL;
static char *virt_path = NULL;

int init_vfs_pool( const char *root_dir )
{
	char *buf;
	size_t rootlen;

	rootlen = strlen( root_dir );

	buf = malloc( FTP_MAX_PATH + rootlen );
	if( buf == NULL )
	{
		FATAL_MEM( FTP_MAX_PATH );
		return FTP_ERROR;
	}

	strlcpy( buf, root_dir, rootlen + 1 );

	real_path = buf;
	virt_path = buf + rootlen;

	memset( virt_path, '\0', FTP_MAX_PATH );
	virt_path[0] = '/';

	return FTP_SUCCESS;
}

int destroy_vfs_pool(void)
{
	free( real_path );
	real_path = virt_path = NULL;
	return FTP_SUCCESS;
}

int failed_vfs_reply( ftp_conn_t *conn )
{
	switch(errno)
	{
	case EPERM:
	case ENOENT:
	case EACCES:
	case ENOTDIR:
	case ENAMETOOLONG:
	case ENOTEMPTY:
	case ELOOP:
	case EUSERS:
	case EDQUOT:
		reply_format(conn, "550 %m\r\n");
		break;
	default:
		log_warn("Unusual vfs error: %m\n");
		reply(conn, "550 Server error\r\n");
		break;
	}

	return FTP_SUCCESS;
}

/* Canonicalize the directory PATH points to, from the directory CWD
 * Don't resolve links, those are problems for later */
static ssize_t vfs_realpath( const char *cwd, const char *path )
{
	char *dst;
	const char *start, *end;
	
	assert( real_path != NULL && virt_path != NULL );

	/* If it's an absolute path, we don't need the cwd */
	if( path[0] == '/' )
	{
		virt_path[0] = '/';
		dst = &virt_path[1];
	} else {
		size_t len;

		len = strlcpy( virt_path, cwd, FTP_MAX_PATH );
		if( len >= FTP_MAX_PATH )
		{
			virt_path[0] = '\0';
			errno = ENAMETOOLONG;
			return -1;
		}
		dst = virt_path + len;
	}
	
	for( start = end = path; *start; start = end )
	{
		ptrdiff_t dirlen;

		/* Skip leading path separators */
		while( *start == '/' )
			++start;
		
		/* Find the end of the current directory block */
		for(end = start; *end && *end != '/'; ++end )
			; /* Do nothing */
			

		dirlen = end - start;

		if( dirlen == 0 )
			break;
		else if( dirlen == 1 && start[0] == '.' )
			;
		else if( dirlen == 2 && start[0] == '.' && start[1] == '.' )
		{
			/* Move up if not at root */
			if( dst > virt_path + 1 )
				while( (--dst)[-1] != '/' )
					; /* Do nothing */
		} else {
			if( dst[-1] != '/' )
				*dst++ = '/';
			
			if(dst + dirlen >= virt_path + FTP_MAX_PATH)
			{
				errno = ENAMETOOLONG;
				return -1;
			}
			
			memcpy( dst, start, dirlen );
			dst += dirlen;
			*dst = '\0';
			
		}
	}
	
	if( dst > virt_path + 1 && dst[-1] == '/' )
		--dst;
	
	*dst = '\0';
	
	return 0;
}

int vfs_stat( const char *cwd, const char *vpath, struct stat *st )
{
	int statret;
	struct stat tmp;

	if( vfs_realpath( cwd, vpath ) == -1 )
		return -1;

	statret = lstat( real_path, &tmp );

	if( statret == -1 )
		return -1;
	else if( S_ISLNK( tmp.st_mode ) && !config.allow_links )
	{
		errno = EACCES;
		return -1;
	}
	
	statret = stat( real_path, st );

	return statret;
}

int vfs_creat( const char *cwd, const char *vpath, mode_t mode )
{
	int ret;

	if( vfs_realpath( cwd, vpath ) == -1 )
		return -1;

	ret = creat( real_path, mode );

	return ret;
}

int vfs_open( const char *cwd, const char *vpath, int flags )
{
	int ret;

	if( vfs_realpath( cwd, vpath ) == -1 )
		return -1;

	ret = open( real_path, flags );

	return ret;
}

int vfs_close( int fd )
{
	return close(fd);
}

int vfs_chdir( char *cwd, const char *path )
{
	if( vfs_realpath( cwd, path ) == -1 )
		return -1;
	
	strlcpy( cwd, virt_path, FTP_MAX_PATH );

	return 0;
}

int vfs_mkdir( const char *cwd, const char *pathname, mode_t mode )
{
	int ret;

	if( vfs_realpath( cwd, pathname ) == -1 )
		return -1;

	ret = mkdir( real_path, mode );

	return ret;
}

int vfs_rmdir( const char *cwd, const char *pathname )
{
	int ret;

	if( vfs_realpath( cwd, pathname ) == -1 )
		return -1;

	ret = rmdir( real_path );

	return ret;
}

DIR *vfs_opendir( const char *cwd, const char *path )
{
	if( vfs_realpath( cwd, path ) == -1 )
		return NULL;
	
	return opendir( real_path );
}

int vfs_closedir( DIR *dirp )
{
	return closedir( dirp );
}

int vfs_unlink( const char *cwd, const char *path )
{
	if( vfs_realpath( cwd, path ) == -1 )
		return -1;

	return unlink( real_path );
}
