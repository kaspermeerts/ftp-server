#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "ftp.h"

enum cfg_type 
{
	TYPE_STR,
	TYPE_INT,
	TYPE_BOOL
};

typedef struct 
{
	const char *name;
	enum cfg_type type;
	void *variable;
} config_table_t ;

static config_table_t core_config_table[] =
{
{ "AllowAnonymous",TYPE_BOOL, &config.allow_anon },
{ "AllowSymlinks", TYPE_BOOL, &config.allow_links },
{ "AnonRootDir",   TYPE_STR,  &config.anon_root_dir },
{ "IdleTimeout",   TYPE_INT,  &config.idle_timeout },
{ "LocalPort",     TYPE_INT,  &config.port},
{ "LogFile",       TYPE_STR,  &config.logfile },
{ "LogToFile",     TYPE_BOOL, &config.log_to_file },
{ "LogToSyslog",   TYPE_BOOL, &config.syslog },
{ "MaxClients",    TYPE_INT,  &config.max_clients},
{ "PasvPortEnd",   TYPE_INT,  &config.pasv_port_end },
{ "PasvPortStart", TYPE_INT,  &config.pasv_port_start },
{ "ServerName",    TYPE_STR,  &config.servername },
{ "TransferRate",  TYPE_INT,  &config.throttle_rate },
{0}
};

/* WARNING: GLOBAL VARIABLE */
/* The function that change this variable are load_config(), load_defaults()
 * and unload_config(). Try to keep the number low */
ftp_config_t config;
const char *config_path = DEFAULT_CFG_PATH;
		
static int parse_config( char *, int, const char * );
static int parse_attribute( char * );
static int parse_attribute_bool( char *, void *);
static int parse_attribute_int( char *, void *);
static int parse_attribute_string( char *, void *);

static int check_config(void);

/* Load the configuration from file
 * It looks like hurdling ^_^ */
int load_config(void)
{
	int fd, len, ret;
	struct stat stat_buf;
	void *filemap;
	const char *filename = config_path;
	
	log_info("Loading configuration from file '%s'\n", filename );

	fd  = open( filename, O_RDONLY );
	if( fd == -1 )
	{
		log_fatal("Unable to open configuration file '%s' (%m)\n", 
				filename);
		return FTP_ERROR;
	}
	
	if( fstat( fd, &stat_buf ) == -1 )
	{
		log_fatal("Unable to determine size of file '%s' (%m)\n", 
				filename);
		close(fd);
		return FTP_ERROR;
	}
	
	if( !S_ISREG( stat_buf.st_mode ) )
	{
		log_fatal("'%s' is not a regular file.\n", filename );
		close(fd);
		return FTP_ERROR;
	}

	if( (len = stat_buf.st_size) == 0 )
	{
		log_warn("File '%s' empty.\n", filename);
		log_warn("Using default configuration\n");
		close(fd);
		return FTP_SUCCESS;
	}
	
	filemap = mmap( NULL, len, PROT_READ, MAP_SHARED, fd, 0 );
	if( filemap == MAP_FAILED )
	{
		log_fatal("Unable to mmap file '%s' (%m)\n", filename );
		close(fd);
		return FTP_ERROR;
	}
	
	ret = parse_config( filemap, len, filename );

	if( ret == FTP_SUCCESS )
		ret = check_config();
		
	if( munmap( filemap, len ) == -1 )
		log_warn("Unable to unmap file '%s' (%m)\n", filename );
		
	close(fd);
	
	return ret;
}

static int parse_config( char *buf, int length, const char *filename )
{
	char *start, *end, *tmp; /* start/end = start/end of current line */
	int tmplen, linelen, linenum, ret;
	bool comment;
	
	start = end = buf;
	linenum = 0;

	/* tmp is a buffer that holds a copy of the current line.
	 * It grows dynamically to hold larger lines but starts at a very
	 * sane default of 80 characters + 1 nul byte, the default
	 * terminal width */
	tmplen = 81;
	tmp = malloc( tmplen );
	if( tmp == NULL )
	{
		FATAL_MEM( tmplen );
		return FTP_ERROR;
	}
	
	end--;

	while( start < buf + length)
	{
		linenum++;
		comment = false;
		/* Start the new line one character after the end newline */
		start = ++end;

		/* Comments start with a pound
		 * Don't forget we still need to find the end of line */
		if( *start == '#' )
			comment = true;
			
		/* Find the end of the line */
		while(	end < buf+length && *end != '\n' )
			end++;
		
		if( end == start || comment ) /* Empty line */
			continue;
		
		linelen = end - start;
		
		if( tmplen <= linelen )
		{
			tmplen = linelen + 1;
			/* reallocing NULL is the same as malloc */
			tmp = realloc( tmp, tmplen );
			if( tmp == NULL )
			{
				/* XXX tmp is not freed if realloc fails */
				FATAL_MEM( tmplen );
				return FTP_ERROR;
			}
		}
		
		memcpy( tmp, start, linelen );
		tmp[linelen] = '\0';
	
		if( (ret = parse_attribute( tmp )) != FTP_PARSER_SUCCESS )
		{
			log_fatal("Error in file %s: line %d: %s\n", 
				filename, linenum, ftp_parse_strerror(ret));
			free(tmp);
			return FTP_ERROR;
		}
	}
	
	free(tmp);
	return FTP_SUCCESS;
}

static int parse_attribute( char *line )
{
	char *argument;
	void *dest;
	config_table_t *option;
	int ret;

	line = trim_whitespace( line );

	/* Skip empty lines */
	if( *line == '\0' )
		return FTP_PARSER_SUCCESS;

	argument = strchr( line, '=' );
	if( !argument )
		return FTP_PARSER_MISSING_ARGUMENT;

	*(argument++) = '\0';

	while( isblank( *argument ) )
		argument++;
	
	if( argument[0] == '\0' ) 
		return FTP_PARSER_MISSING_ARGUMENT;
	
	line = trim_whitespace( line );

	option = core_config_table;

	while( option->name && strcasecmp( option->name, line ) )
		option++;

	if( option->name == NULL )
		return FTP_PARSER_UNKNOWN_OPTION;
	
	dest = option->variable;
	switch( option->type )
	{
	case TYPE_INT:
		ret = parse_attribute_int( argument, dest );
		break;
	case TYPE_BOOL:
		ret = parse_attribute_bool( argument, dest);
		break;
	case TYPE_STR:
		ret = parse_attribute_string( argument, dest );
		break;
	default:
		break;
	}


	return ret;
}

static int parse_attribute_int( char *argument, void *dest )
{
	long value;
	char *inval;
	errno = 0;
	value = strtol( argument, &inval, 0 );
	if( errno == ERANGE || value < INT_MIN || value > INT_MAX )
		return FTP_PARSER_RANGE;
	if( *inval )
		return FTP_PARSER_NAN;

	*(int*)dest = value;

	return FTP_PARSER_SUCCESS;
}

static int parse_attribute_bool( char *argument, void *dest )
{
	char c; bool value;
	c = argument[0];
	if( c == '1' || tolower(c) == 'y' || tolower(c) == 't' )
		value = true;
	else
		value = false;

	*(bool *)dest = value;

	return FTP_PARSER_SUCCESS;
}

static int parse_attribute_string( char *argument, void *dest )
{
	char *newstr;
	free( *(char **)dest );
	argument = trim_whitespace( argument );
	newstr = strdup( argument );
	if( newstr == NULL )
	{
		FATAL_MEM( strlen( argument ) + 1 );
		return FTP_PARSER_NOMEM;
	}
	*(char **)dest = newstr;
	return FTP_PARSER_SUCCESS;
}

const char *ftp_parse_strerror( int err )
{
	switch( err )
	{
	case FTP_PARSER_SUCCESS:
		return "Success"; break;
	case FTP_PARSER_MISSING_ARGUMENT:
		return "Missing argument"; break;
	case FTP_PARSER_UNKNOWN_OPTION:
		return "Unknown option"; break;
	case FTP_PARSER_RANGE:
		return "Out of range"; break;
	case FTP_PARSER_NAN:
		return "Not a number"; break;
	case FTP_PARSER_NOMEM:
		return "Out of memory"; break;
	default:
		return "Unknown error"; break;
	}

}

int load_defaults()
{
	config.port		= DEFAULT_PORT;
	config.ctrl_port	= DEFAULT_CTRL_PORT;
	config.max_clients	= DEFAULT_MAX_CLIENTS;
	config.throttle_rate	= DEFAULT_TRANSFER_RATE;
	config.pasv_port_start	= DEFAULT_PASV_PORT_START;
	config.pasv_port_end	= DEFAULT_PASV_PORT_END;
	config.idle_timeout     = DEFAULT_IDLE_TIMEOUT;
	config.debug		= DEFAULT_DEBUG;
	config.allow_links	= DEFAULT_ALLOW_LINKS;
	config.syslog		= DEFAULT_SYSLOG;
	config.allow_anon	= DEFAULT_ALLOW_ANON;
	config.anon_root_dir	= NULL;
	config.servername	= NULL;

	return 0;
}

int unload_config(void)
{
	config_table_t *option = core_config_table;

	while( option->name )
	{
		if( option->type == TYPE_STR )
		{
			free( *(char **) option->variable );
			*(char **) option->variable = NULL;
		}

		option++;
	}

	return FTP_SUCCESS;
}

static int check_config(void)
{
	if( config.port < 0 || config.port > 65535 )
	{
		log_fatal("Port out of range: %d\n", config.port);
		return FTP_ERROR;
	}

	if( config.ctrl_port < 0 || config.ctrl_port > 65535 )
	{
		log_fatal("Control port out of range: %d\n", 
				config.ctrl_port );
		return FTP_ERROR;
	}

	if( config.throttle_rate < -1 )
	{
		log_fatal("Invalid transfer rate: %d kbps\n",
				config.throttle_rate );
		return FTP_ERROR;
	}

	if(config.pasv_port_start < 0 || config.pasv_port_start > 65535 ||
	   config.pasv_port_end   < 0 || config.pasv_port_end   > 65535 ||
	   config.pasv_port_start > config.pasv_port_end )
	{
		log_fatal("Invalid port range: %d - %d\n",
			config.pasv_port_start, config.pasv_port_end );
		return FTP_ERROR;
	}

	if( config.allow_anon && config.anon_root_dir == NULL )
	{
		log_fatal("No anonymous root directory set\n");
		return FTP_ERROR;
	}

	return FTP_SUCCESS;
}
