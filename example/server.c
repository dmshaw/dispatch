#include <config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dispatch.h>
#include "common.h"

static int
do_msg_1(uint16_t type,struct msg_connection *conn)
{
  printf("I'm in msg_1\n");

  return 0;
}

static int
do_panic(uint16_t type,struct msg_connection *conn)
{
  printf("This is my panic message\n");

  return 0;
}

static struct msg_handler handlers[]=
  {
    {MY_MSG_1,do_msg_1},
    {MSG_TYPE_PANIC,do_panic},
    {0,NULL}
  };

int
main(int argc,char *argv[])
{
  int err;
  struct msg_config config;

  memset(&config,0,sizeof(config));

  config.max_concurrency=1;

  msg_init(&config);

  err=msg_listen(NULL,MY_SOCKET,MSG_LOCAL,handlers);
  if(err==-1)
    {
      fprintf(stderr,"Unable to listen on socket %s: %s\n",
	      MY_SOCKET,strerror(errno));
      return -1;
    }

  printf("Waiting for messages...\n");

  pause();

  return 0;
}
