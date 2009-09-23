/* $Id: conn.h$ */

#ifndef _CONN_H_
#define _CONN_H_

struct msg_connection
{
  int fd;
  struct
  {
    unsigned int internal:1;
    unsigned int poisoned:1;
  } flags;
};

int cloexec_fd(int fd);
struct msg_connection *get_connection(const char *host,const char *service,
				      int flags);
int close_connection(struct msg_connection *conn);

#endif /* !_CONN_H_ */
