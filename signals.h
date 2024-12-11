#ifndef __SIGNAL_H__
#define __SIGNAL_H__ 1

extern unsigned int signal_flag;

extern int init_signals(void);

#define RECV_SIGINT	(1<<0)
#define RECV_SIGTERM	(1<<1)
#define RECV_SIGHUP	(1<<2)
#define RECV_SIGPIPE	(1<<3)
#define RECV_SIGCHLD	(1<<4)
#define RECV_SIGOTHER	(1<<5)

#endif
