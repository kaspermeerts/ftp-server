#include <stdbool.h>
#include <stdio.h>
#include <unistd.h> /* For getopt */

#include "ftp.h"

static int parse_args( int , char **);
static int usage(void);

int main( int argc, char **argv )
{
	int server_socket, ret, pipefds[2];

	if( load_defaults() )
		return 1;

	if( parse_args( argc, argv ) )
		return 1;
	
	if( load_config() )
		return 1;

	if( init_signals() )
		return 1;

	if( start_log() )
		return 1;

	if( init_command_pool() )
		return 1;

	if( init_core_commands() )
		return 1;

	if( init_masterserver(&server_socket, pipefds) )
		return 1;
	
	/* Everything went OK, start the server */
	ret = daemon_main(server_socket, pipefds);		

	close( server_socket );
	
	destroy_command_pool();
	unload_config();

	return ret;
}

/* Parse arguments and fill out the config structure
 * Returns -1 on failure */
static int parse_args( int argc, char **argv )
{
	int c;
	
	while( (c = getopt( argc, argv, ":c:dn")) != -1)
	{
		switch(c)
		{
		case 'c':
			config_path = optarg;
			break;
		case 'd':
			config.debug = true;
			log_dbg("Logging debug information\n");
			break;
		case 'n':
			/*config.nodaemon = true; */
			break;
		case ':':
			fprintf(stderr, "Option -%c requires an operand\n", 
					optopt);
			usage();
			return 1;
			break;
		case '?':
		default: /* Fallthrough */
			fprintf(stderr, "Unknown operand: -%c\n", optopt );
			usage();
			return 1;
			break;
		}
	}
	
	return 0;
}

static int usage()
{
	printf("Usage: %s [-dn] [-c path]\n", PROGNAME);
	printf("   -d\t\tShow debug info\n");
	printf("   -c CONFIG\tUse the configuration file CONFIG\n");
	printf("   -n\t\tDon't become a daemon\n");
	
	return 0;
}
