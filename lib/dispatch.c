static const char RCSID[]="$Id$";

#include <config.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dispatch.h>
#include "conn.h"

typedef int (*msg_handler_t)(unsigned short,struct msg_connection *conn);

struct accept_data
{
  int sock;
  struct msg_handler *handlers;
};

struct dispatch_data
{
  int (*handler)(unsigned short type,struct msg_connection *conn);
  struct msg_connection conn;
  unsigned short type;
};

/* Again, I'm skipping all the connection caching stuff for now.  This
   does only the basic accept/pthread_create/handler/close cycle. */

static msg_handler_t
lookup_handler(struct msg_handler *handlers,unsigned short type)
{
  int i;

  for(i=0;handlers[i].type;i++)
    if(handlers[i].type==type)
      return handlers[i].handler;

  return NULL;
}

static void *
worker_thread(void *d)
{
  struct dispatch_data *ddata=d;

  (ddata->handler)(ddata->type,&ddata->conn);

  close_connection(&ddata->conn);

  free(ddata);

  return NULL;
}

static void
call_panic(struct msg_handler *handlers)
{
  msg_handler_t hand=lookup_handler(handlers,MSG_TYPE_PANIC);
  if(!hand)
    {
      fprintf(stderr,"Unable to handle MSG_TYPE_PANIC\n");
      abort();
    }
}

static void *
accept_thread(void *d)
{
  struct accept_data *adata=d;
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

  for(;;)
    {
      unsigned char header[4];
      ssize_t err;
      pthread_t worker;
      struct dispatch_data *ddata;

      ddata=calloc(1,sizeof(*ddata));
      if(!ddata)
	call_panic(adata->handlers);

      ddata->conn.flags.internal=1;
      ddata->conn.fd=accept(adata->sock,NULL,NULL);
      if(ddata->conn.fd==-1)
	call_panic(adata->handlers);

      if(cloexec_fd(ddata->conn.fd)==-1)
	call_panic(adata->handlers);

      err=msg_read(&ddata->conn,header,4);
      if(err==0)
	{
	  /* EOF, so close and reloop. */

	  close(ddata->conn.fd);
	  free(ddata);
	  continue;
	}
      else if(err==-1)
	call_panic(adata->handlers);

      ddata->type =header[2]<<8;
      ddata->type|=header[3];

      ddata->handler=lookup_handler(adata->handlers,ddata->type);
      if(!ddata->handler)
	{
	  fprintf(stderr,"Unable to handle type %"PRIu16"\n",ddata->type);
	  abort();
	}

      /* Pop off a thread to handle the connection */

      err=pthread_create(&worker,&attr,worker_thread,ddata);
      if(err)
	call_panic(adata->handlers);
    }

  return NULL;
}

int
msg_listen(const char *host,const char *service,int flags,
	   struct msg_handler *handlers)
{
  int i,err,save_errno;
  struct accept_data *data;
  pthread_t thread;

  /* We don't need these yet, so lock them to their correct values. */
  if(host || !(flags&MSG_LOCAL))
    {
      errno=EINVAL;
      return -1;
    }

  data=calloc(1,sizeof(*data));
  if(!data)
    return -1;

  data->sock=-1;

  /* Make up the table to pass to our listener thread */
  for(i=0;handlers[i].type;i++)
    ;

  data->handlers=calloc(1,i*sizeof(struct msg_handler));
  if(!data->handlers)
    goto fail;

  for(i=0;handlers[i].type;i++)
    data->handlers[i]=handlers[i];

  if(flags&MSG_LOCAL)
    {
      struct sockaddr_un addr_un;

      if(strlen(service)+1>sizeof(addr_un.sun_path))
	{
	  errno=ERANGE;
	  goto fail;
	}

      data->sock=socket(AF_LOCAL,SOCK_STREAM,0);
      if(data->sock==-1)
	goto fail;

      if(cloexec_fd(data->sock)==-1)
	goto fail;

      memset(&addr_un,0,sizeof(addr_un));

      addr_un.sun_family=AF_LOCAL;
      strcpy(addr_un.sun_path,service);

      unlink(service);

      err=bind(data->sock,(struct sockaddr *)&addr_un,sizeof(addr_un));
      if(err==-1)
	goto fail;
    }

  err=listen(data->sock,100);
  if(err==-1)
    goto fail;

  /* At this point, we have a handler table and a socket, so let's
     make a thread. */

  if(flags&MSG_NORETURN)
    accept_thread(data);
  else
    {
      err=pthread_create(&thread,NULL,accept_thread,data);
      if(err)
	{
	  errno=err;
	  goto fail;
	}
    }

  return 0;
  
 fail:
  save_errno=errno;
  if(data)
    {
      close(data->sock);
      free(data->handlers);
    }

  free(data);
  errno=save_errno;

  return -1;
}
