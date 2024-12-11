#ifndef __REPLY_H__
#define __REPLY_H__ 1

extern int init_reply_pool(void);
extern int destroy_reply_pool(void);

extern int reply(ftp_conn_t*,const char*);
extern int reply_format( ftp_conn_t *, const char *, ... )
		__attribute__((format(printf,2,3)));

#endif
