#ifndef __FTP_H__ 
#define __FTP_H__ 1
 
#if __GNUC__ >= 3
# define __pure		__attribute__ ((pure))
# define __noreturn	__attribute__ ((noreturn))
# define __malloc	__attribute__ ((malloc))
# define __must_check	__attribute__ ((warn_unused_result))
# define __deprecated	__attribute__ ((deprecated))
# define likely(x)	__builtin_expect (!!(x), 1)
# define unlikely(x)	__builtin_expect (!!(x), 0)
#else
# define __pure		/* no pure */
# define __noreturn	/* no noreturn */
# define __malloc	/* no malloc */
# define __must_check	/* no warn_unused_result */
# define __deprecated	/* no deprecated */
# define likely(x)	(x)
# define unlikely(x)	(x)
#endif


#include "config.h"
#include "daemon.h"
#include "child.h"
#include "log.h"
#include "main.h"
#include "server.h"
#include "util.h"
#include "state.h"
#include "command.h"
#include "throttle.h"
#include "vfs.h"
#include "ls.h"
#include "stream.h"
#include "signals.h"
#include "reply.h"
#include "core.h"
#include "auth.h"

#endif
