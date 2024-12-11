#ifndef __FTPLOG_H__
#define __FTPLOG_H__ 1

extern int start_log(void);
extern int logger(const int, const char *,...)
	__attribute__((format(printf,2,3)));
extern int log_fatal( const char *, ... )__attribute__((format(printf,1,2)));
extern int log_warn( const char *, ... )__attribute__((format(printf,1,2)));
extern int log_info( const char *, ... )__attribute__((format(printf,1,2)));
extern int log_dbg( const char *, ... )__attribute__((format(printf,1,2))); 

#endif /* __FTPLOG_H__ */
