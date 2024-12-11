#ifndef __FTPCOMMAND_H__
#define __FTPCOMMAND_H__	1

#include "server.h"

typedef struct cmd_handler
{
	const char *name;
	int (*handler)( ftp_session_t * );
	bool needs_login;
	bool needs_data;
	bool needs_arg;
	/*bool is_feat;*/
} cmd_handler_t;

extern int init_command_pool(void);
extern int destroy_command_pool(void);
extern int do_cmd( ftp_session_t *);
extern int add_command_list( const cmd_handler_t[] );

/* According to man 3 readdir */
#define D_NAME_MAX	256
#define XFER_BLOCK_SIZE	( (off_t)4096 )

#endif /* __FTPCOMMAND_H__ */
