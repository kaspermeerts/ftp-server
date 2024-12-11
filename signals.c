#include <signal.h>
#include <stdlib.h>

#include "ftp.h"

unsigned int signal_flag;

void signal_handler( int sig )
{
	/* Signals are blocked here */
	switch( sig )
	{
	case SIGINT:
		signal_flag |= RECV_SIGINT;
		break;
	case SIGTERM:
	 	signal_flag |= RECV_SIGTERM;
	 	break;
	case SIGHUP:
	 	signal_flag |= RECV_SIGHUP;
	 	break;
	case SIGPIPE:
	 	signal_flag |= RECV_SIGPIPE;
	 	break;
	case SIGCHLD:
	 	signal_flag |= RECV_SIGCHLD;
	 	break;
	default:
	 	signal_flag |= RECV_SIGOTHER;
	 	break;
	}
	
	return;
}


int init_signals( )
{
	struct sigaction sa;
	sigset_t blockset;
	
	log_dbg("Installing signal handler\n");

	sigfillset( &blockset );
	
	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	sa.sa_mask = blockset;
	
	sigaction( SIGINT,  &sa, NULL );
	sigaction( SIGTERM, &sa, NULL );
	sigaction( SIGHUP,  &sa, NULL );
	sigaction( SIGPIPE, &sa, NULL );
	sigaction( SIGCHLD, &sa, NULL );
	sigaction( SIGUSR1, &sa, NULL );
	
	return 0;
	
}

