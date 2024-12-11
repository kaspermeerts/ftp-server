#ifndef __FTPCONFIG_H__
#define __FTPCONFIG_H__	1

#include <stdbool.h>
#include <stddef.h>

extern int load_config(void);
extern int load_defaults(void);
extern const char *ftp_parse_strerror( int err );
extern int unload_config(void);

enum parse_error
{
	FTP_PARSER_SUCCESS=0,
	FTP_PARSER_MISSING_ARGUMENT,
	FTP_PARSER_UNKNOWN_OPTION,
	FTP_PARSER_RANGE,
	FTP_PARSER_NAN,
	FTP_PARSER_NOMEM
};

typedef struct ftp_config 
{
	int port;
	int ctrl_port;
	int max_clients;
	int throttle_rate;
	int pasv_port_start;
	int pasv_port_end;
	int idle_timeout;
	bool debug;
	bool allow_anon;
	bool allow_links;
	bool log_to_file;
	bool syslog;
	char *anon_root_dir;
	char *servername;
	char *logfile;
} ftp_config_t;

extern const char *config_path;
extern ftp_config_t config;

#define DEFAULT_CFG_PATH	"ftp.conf"

#define DEFAULT_PORT			8008
#define DEFAULT_CTRL_PORT		7676
#define DEFAULT_MAX_CLIENTS		-1
#define DEFAULT_TRANSFER_RATE		-1
#define DEFAULT_PASV_PORT_START		1025
#define DEFAULT_PASV_PORT_END		65535
#define DEFAULT_IDLE_TIMEOUT		4000
#define DEFAULT_ALLOW_ANON		true
#define DEFAULT_DEBUG			false
#define DEFAULT_ALLOW_LINKS		false
#define DEFAULT_SYSLOG			false

#endif /* __FTPCONFIG_H__ */
