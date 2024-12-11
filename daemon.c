#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ftp.h"

static pid_t fork_server(ftp_child_t *, int,int,int*);
static int pre_server( ftp_child_t *head, int socket );
static int handle_client( int, ftp_child_t *, int [] );
static int daemon_handle_signal( ftp_child_t *list );

/* Listen to FTP port, waiting for connections */
int init_masterserver(int *server_socket, int *pipefds )
{
	int sock;
	struct sockaddr_in my_addr;
	socklen_t socklen = (socklen_t) sizeof(struct sockaddr_in);
	
	log_dbg("Initializing masterserver\n");

	sock = socket( AF_INET, SOCK_STREAM, 0 );
	if(sock == -1)
	{
		log_fatal("Unable to open socket: %m\n");
		return FTP_ERROR;
	}
	
	{
		int yes = 1;

		/* Get rid of "Address already in use" message */
		if( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof( yes ) ) == -1 )
			log_warn("Unable to set socket options: %m");
	}

	memset( &my_addr, 0, socklen);
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl( INADDR_ANY );
	my_addr.sin_port = htons( config.port );
	
	if(bind(sock, (struct sockaddr *)&my_addr, socklen) == -1)
	{
		log_fatal("Unable to bind to socket: %m\n");
		close(sock);
		return FTP_ERROR;
	}
	
	if( listen(sock, SOMAXCONN)== -1 )
	{
		log_fatal("Unable to listen to socket: %m\n");
		close(sock);
		return FTP_ERROR;
	}

	if( pipe( pipefds ) == -1 )
	{
		log_fatal("Unable to make a pipe: %m\n");
		close(sock);
		return FTP_ERROR;
	}

	*server_socket = sock;
	
	return FTP_SUCCESS;
}

int daemon_main( int server_socket, int* pipefds )
{
	ftp_child_t *head;
	struct pollfd poll_fd[2];
	int numfds = 2;
	int ret = FTP_SUCCESS;
	
	poll_fd[0].fd		= server_socket;
	poll_fd[0].events	= POLLIN;
	poll_fd[0].revents	= 0;

	poll_fd[1].fd		= pipefds[0];
	poll_fd[1].events	= POLLIN;
	poll_fd[1].revents	= 0;

	log_info("All subsystems loaded, starting FTP server\n");

	/* Head of the linked list */
	if( ( head = malloc( sizeof *head ) ) == NULL )
	{
		FATAL_MEM( sizeof *head );
		return 1;
	}

	head->pid = -1;
	head->next = NULL;

	/* This is the main loop of the server. We wait for incoming
	 * connections or data from the clients. 
	 * We stop when we get a terminating signal */
	while( daemon_handle_signal(head ) == FTP_SUCCESS)
	{
		int poll_ret;

		poll_ret = poll( poll_fd, numfds, -1 );

		if(poll_ret == 0 || ( poll_ret == -1 && errno == EINTR ) )
			continue;
		else if(poll_ret == -1 && errno != EINTR )
		{
			log_fatal("Unable to poll socket: %m\n");
			break;
		}

		if( poll_fd[0].revents & POLLIN )
		{
			ret = handle_client( server_socket, head, pipefds );

			if(ret != FTP_SUCCESS)
				break;
		}

		if( poll_fd[1].revents & POLLIN )
		{
			ret = recv_state( pipefds[0], head );

			if( ret != FTP_SUCCESS )
				break;
		}
	}
	
	remove_all_clients( head, ret != FTP_QUIT );
	free(head);

	if( ret != FTP_QUIT )
		log_info( "FTP daemon shutting down\n");
		
	return 0;
}

/* Returns FTP_SUCCESS the clients forked off successfully
 * Returns FTP_ERROR otherwise
 * Returns FTP_QUIT when the client quits */
int handle_client( int server_sock, ftp_child_t *head, int pipefds[] )
{
	int client_sock;
	pid_t client_pid;
	ftp_child_t *client;
	struct sockaddr_in client_addr;
	socklen_t addrlen;
	addrlen = (socklen_t) sizeof( client_addr );

	client_sock = accept( server_sock,
		(struct sockaddr *) &client_addr, &addrlen );
	if( client_sock == -1 )
	{
		if( errno == EINTR )
			return FTP_SUCCESS;

		log_fatal("Unable to accept connection: %m\n");
		return FTP_ERROR;
	}

	if( pre_server( head, client_sock ) != FTP_SUCCESS )
	{
		/* The pre server initialization failed or the client
		 * was not allowed (load too high, banned host, ... ) */
		log_info( "Refusing connection to %s\n", 
				inet_ntoa( client_addr.sin_addr ) );
		close(client_sock);
		return FTP_SUCCESS;
	}

	if( (client_pid = fork()) == -1 )
	{
		log_fatal("Unable to create new server process: %m\n");
		close( client_sock );
		return 0;
	}

	if( client_pid == 0 )
	{
		/* Close the read end. If there is no reader anymore, the
		 * pipe breaks */
		close( pipefds[0] );
		close( server_sock ); /* We don't need that one, now do we*/
		ftp_main( client_sock, pipefds[1] );
		close( client_sock );
		return FTP_QUIT;
	}
	else
	{
		close( client_sock );
		client = new_client( client_pid, client_addr.sin_addr );
		if( client == NULL )
			return FTP_ERROR;

		return add_client( head, client );
	}
}

int daemon_handle_signal( ftp_child_t *list )
{
	sigset_t blockset, oldset;
	int ret;

	/* Blocking all signal to prevent races */
	sigfillset( &blockset );	
	sigprocmask( SIG_BLOCK, &blockset, &oldset );
	
	if( signal_flag & RECV_SIGCHLD )
	{
		pid_t deadchild;
		
		signal_flag &= !RECV_SIGCHLD;
		
		deadchild = waitpid( -1, NULL, WNOHANG );
		remove_client( list, deadchild );		
	}
	
	if( signal_flag )
	{
		log_info( "Terminating signal caught\n" );
		signal_flag = 0;
		ret = FTP_QUIT;
	}
	else
		ret = FTP_SUCCESS;
	
	sigprocmask( SIG_SETMASK, &oldset, NULL );
	return ret;
}

static int pre_server( ftp_child_t *head, int socket )
{
	int cl_count;
	ftp_conn_t connection;
	connection.sock = socket;

	cl_count = count_clients( head );

	if( config.max_clients != -1 && cl_count >= config.max_clients )
	{
		reply( &connection, "421 Too many clients\r\n" );
		return FTP_ERROR;
	}
	
	return FTP_SUCCESS;
}
