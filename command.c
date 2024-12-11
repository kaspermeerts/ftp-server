#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ftp.h"

static int search_command( const void *, const void *);
static int compare_key_to_cmd( const void *, const void * );
static int add_command( const cmd_handler_t *new_cmd );
static int pre_command( ftp_session_t *session, cmd_handler_t *cmd_def );

#define DEFAULT_CMD_SIZE	(16)

static cmd_handler_t *command_pool = NULL;
static int command_pool_size = 0;

static int num_commands = 0;

int init_command_pool()
{
	command_pool_size = 16;
	command_pool = malloc( 16 * sizeof(cmd_handler_t) );
	if( command_pool == NULL )
	{
		FATAL_MEM( 16 * sizeof(cmd_handler_t) );
		command_pool_size = 0;
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}

int destroy_command_pool()
{
	free(command_pool);
	command_pool = NULL;
	command_pool_size = 0;
	num_commands = 0;
}

static int compare_commands( const void *arg1, const void *arg2 )
{
	cmd_handler_t *cmd1 = (cmd_handler_t *)arg1;
	cmd_handler_t *cmd2 = (cmd_handler_t *)arg2;

	return strcasecmp( cmd1->name, cmd2->name );

}

static int compare_key_to_cmd( const void *arg1, const void *arg2 )
{
	const char *key = (const char *) arg1;
	const cmd_handler_t *command = (const cmd_handler_t *) arg2;

	return strcasecmp( key, command->name );
}

/* Doesn't sort the command list. Use add_command_list or do it yourself */
static int add_command( const cmd_handler_t *new_cmd )
{
	int this_cmd = num_commands;

	if( this_cmd >= command_pool_size )
	{
		command_pool_size *= 2;
		command_pool = realloc( command_pool, 
			command_pool_size * sizeof( cmd_handler_t ) );
		if( command_pool == NULL )
		{
			FATAL_MEM(command_pool_size*sizeof(cmd_handler_t));
			return FTP_ERROR;
		}
	}

	memcpy( &command_pool[this_cmd], new_cmd, sizeof(*new_cmd));
	num_commands++;

	return FTP_SUCCESS;
}

int add_command_list( const cmd_handler_t command_list[] )
{
	while( command_list->name != NULL )
	{
		if( add_command( command_list ) )
			return FTP_ERROR;

		command_list++;
	}

	qsort( command_pool, num_commands, sizeof(cmd_handler_t), 
			compare_commands );

	return FTP_SUCCESS;
}

int do_cmd( ftp_session_t *session )
{
	int ret;
	ftp_conn_t *conn;
	ftp_command_t *command;
	cmd_handler_t *cmd_def;

	conn = &session->conn;
	command = &session->command;

	cmd_def = bsearch( command->line,
			command_pool,
			num_commands,
			sizeof( cmd_handler_t ),
			compare_key_to_cmd );

	if( cmd_def == NULL )
	{
		reply( conn, "500 ?\r\n");
		return FTP_SUCCESS;
	}

	if( pre_command( session, cmd_def ) != FTP_SUCCESS )
		return FTP_SUCCESS;

	ret = cmd_def->handler( session );

	/* TODO: post_command */

	return ret;
}

static int pre_command( ftp_session_t *session, cmd_handler_t *cmd_def )
{
	ftp_conn_t *conn = &session->conn;

	if( cmd_def->needs_login && !session->login.logged_in )
	{
		reply(conn, "530 You're not logged in\r\n");
		return FTP_ERROR;
	}

	if( cmd_def->needs_data && conn->pasv_sock == -1 )
	{
		reply(conn, "425 Cannot open data connection\r\n");
		return FTP_ERROR;
	}

	if( cmd_def->needs_arg && session->command.arg[0] == '\0' )
	{
		reply(conn, "501 Missing argument\r\n");
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}
