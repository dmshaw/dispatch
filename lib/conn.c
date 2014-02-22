#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dispatch.h>
#include "conn.h"

/* No caching yet.  This is all opens and closes. */

socklen_t
populate_sockaddr_un(const char *service,int flags,struct sockaddr_un *addr_un)
{
  socklen_t socklen;

  /* Note that sun_path isn't null terminated.  The socklen field is
     used to know when it ends. */

  memset(addr_un,0,sizeof(addr_un));

  addr_un->sun_family=AF_LOCAL;

  if(flags&MSG_ABSTRACT)
    {
      if(1+strlen(service)>sizeof(addr_un->sun_path))
	{
	  errno=ERANGE;
	  return -1;
	}

      strcpy(&addr_un->sun_path[1],service);

      socklen=offsetof(struct sockaddr_un,sun_path)+1+strlen(service);
    }
  else
    {
      if(1+strlen(service)>sizeof(addr_un->sun_path))
	{
	  errno=ERANGE;
	  return -1;
	}

      strcpy(addr_un->sun_path,service);

      socklen=offsetof(struct sockaddr_un,sun_path)+strlen(service);
    }

  return socklen;
}

int
cloexec_fd(int fd)
{
  long flags;

  flags=fcntl(fd,F_GETFD);
  if(flags==-1)
    return -1;

  flags|=FD_CLOEXEC;

  if(fcntl(fd,F_SETFD,flags)==-1)
    return -1;

  return 0;
}

int
nonblock_fd(int fd)
{
  long flags;

  flags=fcntl(fd,F_GETFL);
  if(flags==-1)
    return -1;

  flags|=O_NONBLOCK;

  if(fcntl(fd,F_SETFL,flags)==-1)
    return -1;

  return 0;
}

struct msg_connection *
get_connection(const char *host,const char *service,int flags)
{
  int err=-1,save_errno;
  struct msg_connection *conn=NULL;

  /* We don't need these yet, so lock them to their correct values. */
  if(host || !(flags&MSG_LOCAL))
    {
      errno=EINVAL;
      return NULL;
    }

  conn=calloc(1,sizeof(*conn));
  if(!conn)
    return NULL;

  conn->fd=-1;

  if(flags&MSG_LOCAL)
    {
      struct sockaddr_un addr_un;
      socklen_t socklen;

      conn->fd=socket(AF_LOCAL,SOCK_STREAM,0);
      if(conn->fd==-1)
	return NULL;

      if(cloexec_fd(conn->fd)==-1)
	goto fail;

      if(flags&MSG_NONBLOCK && nonblock_fd(conn->fd)==-1)
	goto fail;

      socklen=populate_sockaddr_un(service,flags,&addr_un);
      if(socklen==-1)
	goto fail;

      err=connect(conn->fd,(struct sockaddr *)&addr_un,socklen);
    }

  if(err==-1)
    goto fail;

  return conn;

 fail:
  save_errno=errno;
  close(conn->fd);
  free(conn);
  errno=save_errno;
  return NULL;
}

int
close_connection(struct msg_connection *conn)
{
  close(conn->fd);
  if(!conn->flags.internal)
    free(conn);

  return 0;
}
