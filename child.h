#ifndef __FTPLIST_H__
#define __FTPLIST_H__ 1

#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include "server.h"
#include "command.h"

#define MAX_LOGIN_NAME	64

typedef struct ftp_child
{
	struct ftp_child *next;		/* Single linked list */

	pid_t pid;			/* Unique identifier for each
					 * client. */
	time_t connected;
	struct in_addr client_addr;	/* IP address of client */
	char login_name[MAX_LOGIN_NAME];

	char path[FTP_MAX_PATH];
	char filename[FTP_MAX_NAME];
	ftp_xfer_info_t xfer_info;

	bool xfer_in_progress;
} ftp_child_t;

extern __malloc ftp_child_t *new_client( pid_t pid, struct in_addr address );
extern int add_client( ftp_child_t *, ftp_child_t * );
extern int remove_client( ftp_child_t *, pid_t );
extern int remove_all_clients( ftp_child_t *, bool );
extern int count_clients( ftp_child_t * );
extern int count_clients_addr( ftp_child_t *, struct in_addr );
extern ftp_child_t *find_client( ftp_child_t *head, pid_t pid );

#endif /* __FTPLIST_H__ */
