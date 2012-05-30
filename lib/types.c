#include <config.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <dispatch.h>
#include "conn.h"

/* Efficient 1,2,5 length encoding.  Shamelessly borrowed from
   RFC-4880. */

static int
read_length(struct msg_connection *conn,uint32_t *length,uint8_t *special)
{
  unsigned char a;
  ssize_t err;
  uint8_t my_special;

  if(!special)
    special=&my_special;

  *length=*special=0;

  err=msg_read(conn,&a,1);
  if(err!=1)
    return err;

  if(a<192)
    *length=a;
  else if(a<224)
    {
      *length=(a-192)*256;

      err=msg_read(conn,&a,1);
      if(err!=1)
	return err;

      *length+=a+192;
    }
  else if(a<255)
    *special=a&0x1F;
  else
    {
      err=msg_read(conn,&a,1);
      if(err!=1)
	return err;

      *length =a<<24;

      err=msg_read(conn,&a,1);
      if(err!=1)
	return err;

      *length|=a<<16;

      err=msg_read(conn,&a,1);
      if(err!=1)
	return err;

      *length|=a<<8;

      err=msg_read(conn,&a,1);
      if(err!=1)
	return err;

      *length|=a;
    }

  return 1;
}

static int
write_length(struct msg_connection *conn,uint32_t length,uint8_t special)
{
  unsigned char bytes[5];
  size_t do_write;
  ssize_t err;

  if(special)
    {
      bytes[0]=0xE0+(special&0x1F);
      do_write=1;
    }
  else if(length>8383)
    {
      bytes[0]=0xFF;
      bytes[1]=length>>24;
      bytes[2]=length>>16;
      bytes[3]=length>>8;
      bytes[4]=length;
      do_write=5;
    }
  else if(length>191)
    {
      bytes[0]=192+((length-192)>>8);
      bytes[1]=(length-192);
      do_write=2;
    }
  else
    {
      bytes[0]=length;
      do_write=1;
    }

  err=msg_write(conn,bytes,do_write);
  if(err!=do_write)
    return err;
  else
    return 1;
}

/* read_string and write_string return -1 on error, 0 on eof, and >0
   (the length of the string) on success.  There are no short
   reads/writes. */
int
msg_read_string(struct msg_connection *conn,char **string)
{
  uint32_t length;
  int err;
  uint8_t special;

  err=read_length(conn,&length,&special);
  if(err!=1)
    return err;

  if(special==1)
    *string=NULL;
  else
    {
      *string=malloc(length+1);
      if(!*string)
	return -1;

      if(length>0)
	{
	  err=msg_read(conn,*string,length);
	  if(err!=length)
	    free(*string);
	}

      (*string)[length]='\0';
    }

  return err;
}

int
msg_write_string(struct msg_connection *conn,const char *string)
{
  int err;

  if(string)
    {
      size_t length=strlen(string);

      err=write_length(conn,length,0);
      if(err!=1)
	return err;

      if(length>0)
	return msg_write(conn,string,length);
      else
	return 1;
    }
  else
    return write_length(conn,0,1);
}

int
msg_read_buffer_length(struct msg_connection *conn,size_t *length)
{
  int err;
  uint32_t remote_length;

  err=read_length(conn,&remote_length,NULL);
  if(err!=1)
    return err;

  *length=remote_length;

  return err;
}

int
msg_read_buffer(struct msg_connection *conn,void *buffer,size_t length)
{
  if(length>0)
    return msg_read(conn,buffer,length);
  else
    return 1;
}

int
msg_write_buffer_length(struct msg_connection *conn,size_t length)
{
  return write_length(conn,length,0);
}

int
msg_write_buffer(struct msg_connection *conn,const void *buffer,size_t length)
{
  if(length>0)
    return msg_write(conn,buffer,length);
  else
    return 1;
}

int
msg_read_uint8(struct msg_connection *conn,uint8_t *val)
{
  ssize_t err;

  err=msg_read(conn,val,1);
  if(err!=1)
    return err;

  return err;
}

int
msg_write_uint8(struct msg_connection *conn,uint8_t val)
{
  return msg_write(conn,&val,1);
}

int
msg_read_uint16(struct msg_connection *conn,uint16_t *val)
{
  unsigned char buf[2];
  ssize_t err;

  err=msg_read(conn,buf,2);
  if(err!=2)
    return err;

  *val =buf[0]<<8;
  *val|=buf[1];

  return err;
}

int
msg_write_uint16(struct msg_connection *conn,uint16_t val)
{
  unsigned char buf[2];

  buf[0]=val>>8;
  buf[1]=val;

  return msg_write(conn,buf,2);
}

int
msg_read_int32(struct msg_connection *conn,int32_t *val)
{
  unsigned char buf[4];
  ssize_t err;

  err=msg_read(conn,buf,4);
  if(err!=4)
    return err;

  *val =buf[0]<<24;
  *val|=buf[1]<<16;
  *val|=buf[2]<<8;
  *val|=buf[3];

  return err;
}

int
msg_write_int32(struct msg_connection *conn,int32_t val)
{
  unsigned char buf[4];

  buf[0]=val>>24;
  buf[1]=val>>16;
  buf[2]=val>>8;
  buf[3]=val;

  return msg_write(conn,buf,4);
}

int
msg_read_uint32(struct msg_connection *conn,uint32_t *val)
{
  unsigned char buf[4];
  ssize_t err;

  err=msg_read(conn,buf,4);
  if(err!=4)
    return err;

  *val =buf[0]<<24;
  *val|=buf[1]<<16;
  *val|=buf[2]<<8;
  *val|=buf[3];

  return err;
}

int
msg_write_uint32(struct msg_connection *conn,uint32_t val)
{
  unsigned char buf[4];

  buf[0]=val>>24;
  buf[1]=val>>16;
  buf[2]=val>>8;
  buf[3]=val;

  return msg_write(conn,buf,4);
}

int
msg_read_int64(struct msg_connection *conn,int64_t *val)
{
  unsigned char buf[8];
  ssize_t err;

  err=msg_read(conn,buf,8);
  if(err!=8)
    return err;

  *val =(uint64_t)buf[0]<<56;
  *val|=(uint64_t)buf[1]<<48;
  *val|=(uint64_t)buf[2]<<40;
  *val|=(uint64_t)buf[3]<<32;
  *val|=(uint64_t)buf[4]<<24;
  *val|=(uint64_t)buf[5]<<16;
  *val|=(uint64_t)buf[6]<<8;
  *val|=(uint64_t)buf[7];

  return err;
}

int
msg_write_int64(struct msg_connection *conn,int64_t val)
{
  unsigned char buf[8];

  buf[0]=(uint64_t)val>>56;
  buf[1]=(uint64_t)val>>48;
  buf[2]=(uint64_t)val>>40;
  buf[3]=(uint64_t)val>>32;
  buf[4]=(uint64_t)val>>24;
  buf[5]=(uint64_t)val>>16;
  buf[6]=(uint64_t)val>>8;
  buf[7]=(uint64_t)val;

  return msg_write(conn,buf,8);
}

int
msg_read_uint64(struct msg_connection *conn,uint64_t *val)
{
  unsigned char buf[8];
  ssize_t err;

  err=msg_read(conn,buf,8);
  if(err!=8)
    return err;

  *val =(uint64_t)buf[0]<<56;
  *val|=(uint64_t)buf[1]<<48;
  *val|=(uint64_t)buf[2]<<40;
  *val|=(uint64_t)buf[3]<<32;
  *val|=(uint64_t)buf[4]<<24;
  *val|=(uint64_t)buf[5]<<16;
  *val|=(uint64_t)buf[6]<<8;
  *val|=(uint64_t)buf[7];

  return err;
}

int
msg_write_uint64(struct msg_connection *conn,uint64_t val)
{
  unsigned char buf[8];

  buf[0]=val>>56;
  buf[1]=val>>48;
  buf[2]=val>>40;
  buf[3]=val>>32;
  buf[4]=val>>24;
  buf[5]=val>>16;
  buf[6]=val>>8;
  buf[7]=val;

  return msg_write(conn,buf,8);
}

int
msg_read_fd(struct msg_connection *conn,int *fd)
{
  struct msghdr msg={0};
  struct cmsghdr *cmsg;
  char buf[CMSG_SPACE(sizeof(*fd))],i;
  int err;
  struct iovec iov;

  iov.iov_base=&i;
  iov.iov_len=1;
  msg.msg_iov=&iov;
  msg.msg_iovlen=1;
  msg.msg_control=buf;
  msg.msg_controllen=sizeof(buf);

  err=recvmsg(conn->fd,&msg,MSG_CMSG_CLOEXEC);
  if(err==1)
    {
      if(msg.msg_controllen<sizeof(*cmsg))
	return -1;

      for(cmsg=CMSG_FIRSTHDR(&msg);cmsg;cmsg=CMSG_NXTHDR(&msg,cmsg))
	{
	  if(cmsg->cmsg_len!=CMSG_LEN(sizeof(*fd))
	     || cmsg->cmsg_level!=SOL_SOCKET
	     || cmsg->cmsg_type!=SCM_RIGHTS)
	    continue;

	  memcpy(fd,CMSG_DATA(cmsg),sizeof(*fd));
	  break;
	}

      if(!cmsg)
	err=-1;
    }

  return err;
}

int
msg_write_fd(struct msg_connection *conn,int fd)
{
  struct msghdr msg={0};
  struct cmsghdr *cmsg;
  char buf[CMSG_SPACE(sizeof(fd))]={0};
  struct iovec iov;

  iov.iov_base="i";
  iov.iov_len=1;
  msg.msg_iov=&iov;
  msg.msg_iovlen=1;
  msg.msg_control=buf;
  msg.msg_controllen=sizeof(buf);
  cmsg=CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level=SOL_SOCKET;
  cmsg->cmsg_type=SCM_RIGHTS;
  cmsg->cmsg_len=CMSG_LEN(sizeof(fd));
  memcpy(CMSG_DATA(cmsg),&fd,sizeof(fd));

  return sendmsg(conn->fd,&msg,0);
}
