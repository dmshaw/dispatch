#ifndef _CONN_H_
#define _CONN_H_

#include <sys/un.h>

struct msg_connection
{
  int fd;
  int flags;
  struct
  {
    unsigned int internal:1;
  } bits;
};

socklen_t populate_sockaddr_un(const char *service,struct sockaddr_un *addr_un);
int cloexec_fd(int fd);
int nonblock_fd(int fd);
struct msg_connection *get_connection(const char *host,const char *service,int flags);
int close_connection(struct msg_connection *conn);
int conn_peerinfo(struct msg_connection *conn,struct msg_peerinfo *info);

#endif /* !_CONN_H_ */
