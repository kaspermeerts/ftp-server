#ifndef __FTPSERVER_H__
#define __FTPSERVER_H__ 1
#include <netinet/in.h>
#include <stdbool.h>
#include "child.h"

#define COMMAND_BUFFER_SIZE	1024
#define FTP_MAX_PATH		1024
#define FTP_MAX_NAME		256

extern int pasv_listen( int, char[] );
extern int ftp_main(int, int);
extern __must_check int server_handle_signal(void);

enum
{
	FTP_ERROR = -1,		/* Error */
	FTP_SUCCESS = 0,	/* Success */
	FTP_QUIT,		/* End of the connection */
	FTP_FAIL,		/* Command failed, non-fatally */
	FTP_ABOR,
};

typedef struct 
{
	char *line;		/* Line buffer */
	size_t line_len;	/* Size of line buffer */
	size_t dirty_len;	/* How much of the data hasn't been
				 * parsed yet */
	char *arg;		/* Points at the argument inside line */
	size_t len;
} ftp_command_t;

typedef struct ftp_conn
{
	int master_pipe;		/* Pipe for communication with
					 * masterserver */
	int sock;			/* Socket of control connection */
	int pasv_sock;			/* Socket of data connection */
	int data_sock;
	struct in_addr host_addr;	/* Our IP address */
	struct in_addr client_addr;	/* IP address of client */
} ftp_conn_t;

typedef struct ftp_login
{
	bool logged_in;			/* Self explanatory */
	bool anonymous;			/* Is the user anonymous */
	char *user;
} ftp_login_t;

/* 64 bits is enough to keep track of 16 EB.
 * Even if we transferred at 1 GB/s, it would take 544 years to flow over. */
typedef struct ftp_xfer_info
{
	uint64_t total_down;
	uint64_t total_up;

	uint64_t xfer_len;
	uint64_t probe_len;

	int xfer_status;

	struct timeval xfer_start;
	struct timeval xfer_probe;
} ftp_xfer_info_t;


/* Big session object. It holds all the information the server needs. */
typedef struct 
{
	ftp_conn_t	conn;
	ftp_command_t	command;
	ftp_login_t	login;
	ftp_xfer_info_t	info;

	char *virt_path;
	char *filename;
	off_t restart_pos;
} ftp_session_t;

#endif /* __FTPSERVER_H__ */
