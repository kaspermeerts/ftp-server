#ifndef __VFS_H__
#define __VFS_H__ 1

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

extern int init_vfs_pool(const char *);
extern int destroy_vfs_pool(void);
extern int failed_vfs_reply( ftp_conn_t *conn );

extern int vfs_stat(const char *, const char *, struct stat * );
extern int vfs_creat( const char *cwd, const char *vpath, mode_t );
extern int vfs_open(const char *, const char *, int );
extern int vfs_close( int );
extern int vfs_chdir( char *cwd, const char *path );
extern int vfs_mkdir( const char *, const char *, mode_t );
extern int vfs_rmdir( const char *cwd, const char *pathname );
extern __must_check DIR *vfs_opendir( const char *cwd, const char *path );
extern int vfs_closedir( DIR *dirp );
extern int vfs_unlink( const char *, const char * );

#endif
