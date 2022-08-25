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
populate_sockaddr_un(const char *service,struct sockaddr_un *addr_un)
{
  socklen_t socklen;
  size_t servicelen;

  if(!service)
    {
      errno=EINVAL;
      return -1;
    }

  servicelen=strlen(service);

  if(servicelen<2)
    {
      errno=EINVAL;
      return -1;
    }

  memset(addr_un,0,sizeof(*addr_un));

  addr_un->sun_family=AF_LOCAL;

  if(service[0]=='@')
    {
      /* Note that sun_path isn't null terminated in the abstract
         namespace.  The socklen field is used to know when it ends.
         This code effectively replaces the leading @ in the service
         with a null. */

      if(servicelen>sizeof(addr_un->sun_path))
        {
          errno=ERANGE;
          return -1;
        }

      memcpy(&addr_un->sun_path[1],&service[1],servicelen-1);

      socklen=offsetof(struct sockaddr_un,sun_path)+servicelen;
    }
  else
    {
      /* There is actually some minor debate whether sun_path needs to
         be null terminated for regular local sockets, so this code
         terminates it just to be safe. */

      if(servicelen+1>sizeof(addr_un->sun_path))
        {
          errno=ERANGE;
          return -1;
        }

      strcpy(addr_un->sun_path,service);

      socklen=offsetof(struct sockaddr_un,sun_path)+servicelen+1;
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
  if(host || !service || strlen(service)<2
     || !(service[0]=='/' || service[0]=='@'))
    {
      errno=EINVAL;
      return NULL;
    }

  conn=calloc(1,sizeof(*conn));
  if(!conn)
    return NULL;

  conn->fd=-1;

  if(service[0]=='/' || service[0]=='@')
    {
      struct sockaddr_un addr_un;
      socklen_t socklen;

      conn->fd=socket(AF_LOCAL,SOCK_STREAM,0);
      if(conn->fd==-1)
        goto fail;

      if(cloexec_fd(conn->fd)==-1)
        goto fail;

      conn->flags=flags;

      if(flags&MSG_NONBLOCK && nonblock_fd(conn->fd)==-1)
        goto fail;

      socklen=populate_sockaddr_un(service,&addr_un);
      if(socklen==-1)
        goto fail;

      err=connect(conn->fd,(struct sockaddr *)&addr_un,socklen);
    }

  if(err==-1)
    goto fail;

  return conn;

 fail:
  save_errno=errno;
  if(conn->fd>-1)
    close(conn->fd);
  free(conn);
  errno=save_errno;
  return NULL;
}

int
close_connection(struct msg_connection *conn)
{
  close(conn->fd);
  if(!conn->bits.internal)
    free(conn);

  return 0;
}

int
conn_peerinfo(struct msg_connection *conn,struct msg_peerinfo *info)
{
#ifdef HAVE_STRUCT_UCRED
  struct ucred ucred;
  socklen_t len=sizeof(ucred);

  if(getsockopt(conn->fd,SOL_SOCKET,SO_PEERCRED,&ucred,&len)==-1)
    return -1;

  info->type=MSG_PEERINFO_LOCAL;
  info->local.pid=ucred.pid;
  info->local.uid=ucred.uid;
  info->local.gid=ucred.gid;

  return 0;
#else
  errno=EINVAL;

  return -1;
#endif
}
