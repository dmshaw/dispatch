#include <config.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <dispatch.h>
#include "conn.h"

extern struct msg_config *_config;
static pthread_mutex_t concurrency_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t concurrency_cond=PTHREAD_COND_INITIALIZER;
static size_t concurrency;

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

static int
internal_ping(uint16_t type,struct msg_connection *conn)
{
  return msg_write_uint8(conn,0);
}

static msg_handler_t
lookup_handler(struct msg_handler *handlers,unsigned short type)
{
  int i;

  for(i=0;handlers[i].type;i++)
    if(handlers[i].type==type)
      return handlers[i].handler;

  if(type==MSG_TYPE_PING)
    return internal_ping;

  return NULL;
}

static void *
worker_thread(void *d)
{
  struct dispatch_data *ddata=d;

  (ddata->handler)(ddata->type,&ddata->conn);

  close_connection(&ddata->conn);

  free(ddata);

  pthread_mutex_lock(&concurrency_lock);
  concurrency--;
  pthread_cond_signal(&concurrency_cond);
  pthread_mutex_unlock(&concurrency_lock);

  return NULL;
}

#ifdef __linux__
static void
dump_status(FILE *output)
{
  pid_t pid=getpid();
  char path[1024];

  if(snprintf(path,1024,"/proc/%u/status",pid)>1024)
    fprintf(output,"Can't make path\n");
  else
    {
      FILE *file;

      file=fopen(path,"r");
      if(!file)
	fprintf(output,"Can't open %s: %s\n",path,strerror(errno));
      else
	{
	  char line[1024];

	  while(fgets(line,1024,file))
	    fprintf(output,"%s",line);

	  fclose(file);
	}
    }
}
#endif

static void
call_panic(struct msg_handler *handlers,const char *where,const char *error)
{
  msg_handler_t hand=lookup_handler(handlers,MSG_TYPE_PANIC);

  syslog(LOG_DAEMON|LOG_EMERG,"Dispatch PANIC!  Location: %s  Concurrency:"
	 " %u of %u  Error: %s",where?where:"<NULL>",
	 (unsigned int)concurrency,(unsigned int)_config->max_concurrency,
	 error?error:"<NULL>");

  fprintf(stderr,"Dispatch PANIC!  Location: %s  Concurrency: %u of %u"
	  "  Error: %s\n",where?where:"<NULL>",(unsigned int)concurrency,
	  (unsigned int)_config->max_concurrency,error?error:"<NULL>");

  if(hand)
    (hand)(MSG_TYPE_PANIC,NULL);
  else
    {
#ifdef __linux__
      dump_status(stderr);
#endif
    }

  abort();
}

static void *
accept_thread(void *d)
{
  struct accept_data *adata=d;
  pthread_attr_t attr;
  unsigned int failed_accept_count=0;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  if(_config->stacksize)
    {
      int err=pthread_attr_setstacksize(&attr,_config->stacksize);
      if(err)
	call_panic(adata->handlers,"pthread_attr_setstacksize",strerror(err));
    }

  for(;;)
    {
      unsigned char header[4];
      ssize_t err;
      pthread_t worker;
      struct dispatch_data *ddata;

      ddata=calloc(1,sizeof(*ddata));
      if(!ddata)
	call_panic(adata->handlers,"calloc",strerror(errno));

      ddata->conn.bits.internal=1;

      do
	{
	  ddata->conn.fd=accept(adata->sock,NULL,NULL);
	  if(ddata->conn.fd==-1 && errno!=EINTR)
	    {
	      if(_config->panic_on.failed_accept)
		call_panic(adata->handlers,"accept",strerror(errno));
	      else if(_config->log_on.failed_accept
		      && (failed_accept_count++)%_config->log_on.failed_accept==0)
		syslog(LOG_DAEMON|LOG_ERR,"Dispatch could not accept: %s",
		       strerror(errno));
	    }
	}
      while(ddata->conn.fd==-1);

      if(cloexec_fd(ddata->conn.fd)==-1)
	call_panic(adata->handlers,"cloexec",strerror(errno));

      pthread_mutex_lock(&concurrency_lock);

      while(concurrency>=_config->max_concurrency)
	pthread_cond_wait(&concurrency_cond,&concurrency_lock);

      concurrency++;

      pthread_mutex_unlock(&concurrency_lock);

      err=msg_read(&ddata->conn,header,4);
      if(err==0)
	{
	  /* EOF, so close and reloop. */

	  close(ddata->conn.fd);
	  free(ddata);
	  continue;
	}
      else if(err==-1)
	call_panic(adata->handlers,"msg_read",strerror(errno));

      ddata->type =header[2]<<8;
      ddata->type|=header[3];

      ddata->handler=lookup_handler(adata->handlers,ddata->type);
      if(!ddata->handler)
	{
	  syslog(LOG_DAEMON|LOG_EMERG,"Unable to handle type %"PRIu16,
		 ddata->type);

	  fprintf(stderr,"Unable to handle type %"PRIu16"\n",ddata->type);

	  abort();
	}

      /* Pop off a thread to handle the connection */

      err=pthread_create(&worker,&attr,worker_thread,ddata);
      if(err)
	call_panic(adata->handlers,"pthread_create",strerror(err));
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
  if(host || !service || strlen(service)<2
     || !(service[0]=='/' || service[0]=='@'))
    {
      errno=EINVAL;
      return -1;
    }

  if(!_config)
    {
      static struct msg_config my_config;

      msg_config_init(&my_config);
      _config=&my_config;
    }

  data=calloc(1,sizeof(*data));
  if(!data)
    return -1;

  data->sock=-1;

  /* Make up the table to pass to our listener thread */
  for(i=0;handlers[i].type;i++)
    ;

  data->handlers=calloc(1,(i+1)*sizeof(struct msg_handler));
  if(!data->handlers)
    goto fail;

  for(i=0;handlers[i].type;i++)
    data->handlers[i]=handlers[i];

  data->handlers[i].type=0;

  if(service[0]=='/' || service[0]=='@')
    {
      struct sockaddr_un addr_un;
      socklen_t socklen;

      data->sock=socket(AF_LOCAL,SOCK_STREAM,0);
      if(data->sock==-1)
	goto fail;

      if(cloexec_fd(data->sock)==-1)
	goto fail;

      socklen=populate_sockaddr_un(service,&addr_un);
      if(socklen==-1)
	goto fail;

      if(service[0]=='/')
	unlink(service);

      err=bind(data->sock,(struct sockaddr *)&addr_un,socklen);
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
