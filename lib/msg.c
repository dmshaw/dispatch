#include <config.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dispatch.h>
#include "conn.h"

struct msg_config *_config;

void
msg_config_init(struct msg_config *config)
{
  memset(config,0,sizeof(*config));

  config->max_concurrency=-1;
  config->panic_on.failed_accept=1;
}

int
msg_init(struct msg_config *config)
{
  _config=malloc(sizeof(*_config));
  if(!_config)
    {
      errno=ENOMEM;
      return -1;
    }

  memcpy(_config,config,sizeof(*_config));

  if(_config->max_concurrency==0)
    _config->max_concurrency=-1;

  return 0;
}

/* Sending side. */

/* Make a connection to the specified service. */
struct msg_connection *
msg_open(const char *host,const char *service,int flags)
{
  struct msg_connection *conn;

  conn=get_connection(host,service,flags);

  if(conn)
    {
      unsigned char header[2]={1,0};
      int ret;

      ret=msg_write(conn,header,2);
      if(ret<1)
	{
	  msg_poison(conn);
	  msg_close(conn);
	  conn=NULL;
	}
    }

  return conn;
}

/* Read that never returns a short count.  It either succeeds
   completely, or fails completely. */

ssize_t
msg_read(struct msg_connection *conn,void *buf,size_t count)
{
  size_t do_read=count;
  char *read_to=buf;

  while(do_read)
    {
      ssize_t did_read;

      did_read=read(conn->fd,read_to,do_read);

      if(did_read==-1)
	return -1;

      if(did_read==0)
	return 0;

      do_read-=did_read;
      read_to+=did_read;
    }

  return count;
}

/* Same thing, for write. */

ssize_t
msg_write(struct msg_connection *conn,const void *buf,size_t count)
{
  size_t do_write=count;
  const char *write_to=buf;

  while(do_write)
    {
      ssize_t did_write;

      did_write=write(conn->fd,write_to,do_write);

      if(did_write==-1)
	return -1;

      if(did_write==0)
	return 0;

      do_write-=did_write;
      write_to+=did_write;
    }

  return count;
}

/* No effect in this version as we don't have caching yet. */
int
msg_poison(struct msg_connection *conn)
{
  return 0;
}

int
msg_close(struct msg_connection *conn)
{
  if(conn)
    return close_connection(conn);
  else
    return 0;
}
