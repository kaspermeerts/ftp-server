#ifndef __AUTH_H__
#define __AUTH_H__ 1

extern int doacct( ftp_session_t *session );
extern int douser (ftp_session_t *session);
extern int dopass (ftp_session_t *session);

#define PASSWORD_BUFFER_SIZE	( (size_t) 4096 )

#endif
