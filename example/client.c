#include <config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dispatch.h>
#include "common.h"

int
main(int argc,char *argv[])
{
  struct msg_connection *conn;
  int err;

  printf("Sending message 1...\n");

  conn=msg_open(NULL,MY_SOCKET,MSG_LOCAL);
  if(!conn)
    {
      fprintf(stderr,"Unable to open socket %s: %s\n",
	      MY_SOCKET,strerror(errno));
      goto fail;
    }

  err=msg_write_type(conn,MY_MSG_1);
  if(err<1)
    goto fail;

 fail:
  msg_close(conn);

  return 0;
}
