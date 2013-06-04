#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <sys/types.h>

/* Some types */
struct msg_connection;

/* The handler callback should return 0 for success, and -1 for
   failure. */
struct msg_handler
{
  uint16_t type;
  int (*handler)(uint16_t type,struct msg_connection *conn);
};

struct msg_config
{
  size_t max_concurrency;
  size_t stacksize;
  struct
  {
    unsigned int failed_accept:1;
  } panic_on;
  struct
  {
    unsigned int failed_accept;
  } log_on;
};

/* Fill in a msg_config structure with the default values. */

void msg_config_init(struct msg_config *config);

/* Call this before any other message functions.  If you don't call
   it, the default configuration will be used. */

int msg_init(const struct msg_config *config);

/* Open a connection to the entity specified via host & service (in
   the getaddrinfo sense). */

struct msg_connection *msg_open(const char *host,const char *service,int flags);

/* Possible flags are:

   Use local sockets.  Host must be NULL.  Service contains the full
   path to the local/unix domain socket. */
#define MSG_LOCAL 1

/* Don't return on a listen (i.e. become the listener thread) */
#define MSG_NORETURN 2

/* Don't block on a send */
#define MSG_NONBLOCK 4

/* TODO: add the getaddrinfo flags here, a la NUMERICHOST, etc. */

/* Read and write to an open connection.  Treat these as you would
   read() and write(), except that msg_write is guaranteed to write
   all of its buffer and will never return a short count. */

ssize_t msg_read(struct msg_connection *conn,void *buf,size_t count);
ssize_t msg_write(struct msg_connection *conn,const void *buf,size_t count);

/* "Poison" a connection, so when msg_close() is called on it, the
   connection will be forced closed and never cached.  Note that
   msg_poison() does not close the connection itself, but simply marks
   the connection so that msg_close() will force the close. */

int msg_poison(struct msg_connection *conn);

/* Close an open connection.  Note that msg may choose to cache this
   open connection for future use, so the actual socket may not be
   closed.  Either way, the conn pointer cannot be used again. */

int msg_close(struct msg_connection *conn);

/* Listen on host/service. Same flags as msg_open. */
int msg_listen(const char *host,const char *service,int flags,
	       struct msg_handler *handlers);

/* The handler function should return 1 for success, and -1 for failure. */

#define msg_read_type(_c,_v) msg_read_uint16(_c,_v)
#define msg_write_type(_c,_v) msg_write_uint16(_c,_v)

/* Some predefined types */
#define MSG_TYPE_RESERVED 0
#define MSG_TYPE_PING     65534
#define MSG_TYPE_PANIC    65535  /* Must not return */

/* Some handy readers and writers */

/* These read/write functions return -1 on error, 0 on eof, and >0
   (the length of the string or integer) on success.  There are no
   short reads/writes. */

int msg_read_string(struct msg_connection *conn,char **string);
int msg_write_string(struct msg_connection *conn,const char *string);

int msg_read_buffer_length(struct msg_connection *conn,size_t *length);
int msg_read_buffer(struct msg_connection *conn,void *buffer,size_t length);

int msg_write_buffer_length(struct msg_connection *conn,size_t length);
int msg_write_buffer(struct msg_connection *conn,
		     const void *buffer,size_t length);

int msg_read_uint8(struct msg_connection *conn,uint8_t *val);
int msg_write_uint8(struct msg_connection *conn,uint8_t val);

int msg_read_uint16(struct msg_connection *conn,uint16_t *val);
int msg_write_uint16(struct msg_connection *conn,uint16_t val);

int msg_read_int32(struct msg_connection *conn,int32_t *val);
int msg_write_int32(struct msg_connection *conn,int32_t val);

int msg_read_uint32(struct msg_connection *conn,uint32_t *val);
int msg_write_uint32(struct msg_connection *conn,uint32_t val);

int msg_read_int64(struct msg_connection *conn,int64_t *val);
int msg_write_int64(struct msg_connection *conn,int64_t val);

int msg_read_uint64(struct msg_connection *conn,uint64_t *val);
int msg_write_uint64(struct msg_connection *conn,uint64_t val);

int msg_read_fd(struct msg_connection *conn,int *fd);
int msg_write_fd(struct msg_connection *conn,int fd);

#ifdef __cplusplus
}
#endif

#endif /* !_DISPATCH_H_ */
