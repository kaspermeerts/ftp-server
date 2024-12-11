#ifndef __CORE_H__
#define __CORE_H__

extern int dosyst (ftp_session_t *session);
extern int donoop (ftp_session_t *session);
extern int dotype (ftp_session_t *session);
extern int douser (ftp_session_t *session);
extern int dopwd  (ftp_session_t *session);
extern int docwd  (ftp_session_t *session);
extern int docdup (ftp_session_t *session);
extern int dopasv (ftp_session_t *session);
extern int dosize (ftp_session_t *session);
extern int domdtm (ftp_session_t *session);
extern int doquit (ftp_session_t *session);
extern int dofeat (ftp_session_t *session);
extern int doopts (ftp_session_t *session);
extern int doabor (ftp_session_t *session);
extern int dorest (ftp_session_t *session);
extern int doretr (ftp_session_t *session);
extern int doclnt (ftp_session_t *session);
extern int domkd  (ftp_session_t *session);
extern int dormd  (ftp_session_t *session);
extern int doallo (ftp_session_t *session);
extern int dostor (ftp_session_t *session);
extern int dodele (ftp_session_t *session);

extern int init_core_commands(void);

#endif
