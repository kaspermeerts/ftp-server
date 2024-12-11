#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "ftp.h"

static int destroy_client( ftp_child_t *child );
static int log_stats( ftp_child_t *child );

/* It's not that much right now, just a simple linked list */
int add_client( ftp_child_t *list, ftp_child_t *new )
{
	/* Find the end of the list */
	while( list->next )
		list = list->next;
		
	list->next = new;
		
	return 0;
}

ftp_child_t *new_client( pid_t client_pid, struct in_addr address )
{
	ftp_child_t *child;

	log_dbg("Adding child with PID %d\n", client_pid );

	child = malloc( sizeof( *child ));
	if( child == NULL )
	{
		FATAL_MEM(sizeof( *child ) );
		return NULL;
	}
		
	memset( child, '\0', sizeof( *child ) );

	child->next = NULL;
	child->pid = client_pid;
	child->client_addr = address;
	child->connected = time(NULL);
	child->login_name[0] = '\0';
	child->xfer_in_progress = false;

	return child;
	
}

static int destroy_client( ftp_child_t *child )
{
	free( child );
	return FTP_SUCCESS;
}

int remove_client( ftp_child_t *list, pid_t client_pid )
{
	ftp_child_t *prev;
	
	while( list )
	{
		prev = list;
		list = list->next;

		if( list->pid == client_pid )
		{
			log_stats( list );
			prev->next = list->next;
			destroy_client( list );
			return 0;
		}
	}
	
	log_warn("Client with PID %ld not found\n", (long) client_pid );
	return 1;
}

int count_clients( ftp_child_t *head )
{
	int i;
	ftp_child_t *list;
	
	list = head->next;

	for( i = 0; list; list = list->next )
		i++;
		
	return i;
}

int count_clients_addr( ftp_child_t *list, struct in_addr cl_addr )
{
	int i = 0;

	for( list = list->next; list; list = list->next )
		if( list->client_addr.s_addr == cl_addr.s_addr )
			i++;
	
	return i;
}

int remove_all_clients( ftp_child_t *head, bool kill_clients )
{
	/* Kill all children */
	ftp_child_t *child;
	ftp_child_t *next;

	child = head->next;
	while( child )
	{
		if( kill_clients)
			kill( child->pid, SIGTERM );
		next = child->next;
		destroy_client( child );
		child = next;
	}

	return FTP_SUCCESS;
}

ftp_child_t *find_client( ftp_child_t *head, pid_t pid )
{
	ftp_child_t *child;

	for( child = head->next; child; child = child->next )
	{
		if( child->pid == pid )
			return child;
	}

	return NULL;
}

static int log_stats( ftp_child_t *child )
{
	int diff;

	diff = time(NULL) - child->connected;
	log_info("The client from %s was online for %u seconds\n",
		inet_ntoa( child->client_addr), diff );
	log_info("The client transferred %llu kilobytes\n", 
		(unsigned long long) child->xfer_info.total_down / 1024 );

	return FTP_SUCCESS;
}
