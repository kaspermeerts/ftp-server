#ifndef __STATE_H__
#define __STATE_H__	1

#include <netinet/in.h>
#include <sys/types.h>
#include "command.h"
#include "server.h"
#include "child.h"

enum ftp_statechange_type {
	T_LOGIN,
	T_CHDIR,
	T_XFER_START,
	T_XFER,
	T_XFER_STOP,
};

typedef struct ftp_state
{
	pid_t pid;
	unsigned int magic;
	int type;
} ftp_state_t;

#define STATE_MAGIC	(0xDEADBEEF)

extern int init_state_pool(void);
extern int destroy_state_pool(void);

extern int send_state( ftp_session_t *, int type );
extern int recv_state( int read_pipe, ftp_child_t *);

#endif 

