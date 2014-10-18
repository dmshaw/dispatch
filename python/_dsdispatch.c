/*************************************************************************
 * Python wrapper for the Dispatch C-API
 *
 * This Python C-API creates a binding for the dispatch library to
 * allow it to be used from Python.
 * 
 * Copyright (c) 2014  John Mulligan <johnm@asynchrono.us>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <Python.h>
#include <dispatch.h>
#include <sys/socket.h>
#include <sys/un.h>

/* private in-tree headers */
#include <conn.h>

#define MODULE_NAME "_dsdispatch"


#ifdef USEDEBUGP
#define Debugp(format, ...) fprintf (stderr, "[%d] %s() : " format "\n", getpid(), __func__, ##__VA_ARGS__)
#else
#define Debugp(...)
#endif

static int listen_socket(int *sock, const char *host, const char *service,
                         int flags);
int msg_conn_accept(struct msg_connection *conn, int sock);
static PyObject *raise_closed(void);


static int
listen_socket(int *sock, const char *host, const char *service, int flags)
{
    int err = 0;
    int opened = 0;
    struct sockaddr_un addr_un;
    int _sock = -1;

    if(host || !(flags & MSG_LOCAL)) {
        Debugp("!!! Invalid Arguments");
        errno = EINVAL;
        return -1;
    }
    if ((strlen(service) + 1) > sizeof(addr_un.sun_path)) {
        errno = ERANGE;
        goto fail;
    }

    if(service[0]=='/' || service[0]=='@') {
        struct sockaddr_un addr_un;
        socklen_t socklen;

        Debugp("service: %s", service);
        _sock = socket(AF_LOCAL, SOCK_STREAM, 0);
        if(_sock==-1)
            goto fail;
        opened = 1;
        *sock = _sock;

        if(cloexec_fd(_sock)==-1)
            goto fail;

        socklen = populate_sockaddr_un(service, &addr_un);
        if(socklen == -1)
            goto fail;

        if(service[0]=='/')
            unlink(service);

        err = bind(_sock, (struct sockaddr *)&addr_un, socklen);
        if(err==-1)
            goto fail;
    } else {
        Debugp("not a path");
    }

    err = listen(_sock, 100);
    if(err == -1)
        goto fail;

    return 0;
fail:
    Debugp("failed");
    if (opened) close(_sock);
    return -1;
}


int
msg_conn_accept(struct msg_connection *conn, int sock)
{
    Debugp("got socket: %d", sock);
    do {
        conn->fd = accept(sock, NULL, NULL);
        if (conn->fd == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
    } while (conn->fd == -1);
    return 0;
}


/* ********************************************************** */



/* START Connection Object */

typedef struct {
    PyObject_HEAD
    /* object specific fields */
    struct msg_connection *conn;
} MsgConnection;

static PyObject *Connection_close(MsgConnection *self, PyObject *args);

static PyObject *
Connection_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    MsgConnection *self = (MsgConnection *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;
    self->conn = NULL;
    Debugp("Created new Connection obj: %p", (void*)self);
    return (PyObject *)self;
}


static void
Connection_dealloc(MsgConnection *self)
{
    Debugp("Dealloc connection: %p (conn=%p)",
           (void*)self, (void*)(self->conn));
    Connection_close(self, NULL);
    self->ob_type->tp_free((PyObject *)self);
}


static PyObject *
Connection_open(PyTypeObject *type, PyObject *args, PyObject* kwargs)
{
    char *host;
    char *service;
    int flags = 0;
    MsgConnection *self = (MsgConnection *)Connection_new(type, args, kwargs);
    if (!PyArg_ParseTuple(args, "ss|i", &host, &service, &flags)) {
        return NULL;
    }
    Debugp("Args= %s, %s, %d", host, service, flags);
    if (strlen(host) == 0) {
        /* set empty string back to none for dispatch call */
        host = NULL;
    }

    self->conn = msg_open(host, service, flags);
    if (self->conn == NULL) {
        Debugp("failed to get connection");
        PyErr_SetFromErrno(PyExc_OSError);
        Connection_dealloc(self);
        return NULL;
    }
    Debugp("created connection. conn=%p", (void*)self->conn);
    return (PyObject *)self;
}


PyDoc_STRVAR(Connection_open_doc,
"open(host, service [, flags]) -> connection\n\
\n\
Open a new connection to the specified host and service.\n\
NOTE: Currently host must always be an empty string as only\n\
unix domain sockets are supported.");


static PyObject *
Connection_accept(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    int sock;
    int err;
    struct msg_connection *conn = NULL;

    if (!PyArg_ParseTuple(args, "i", &sock)) {
        return NULL;
    }
    conn = calloc(1, sizeof(*conn));
    if (!conn) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    err = msg_conn_accept(conn, sock);
    Py_END_ALLOW_THREADS
    if (err) {
        free(conn);
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    MsgConnection *self = (MsgConnection *)Connection_new(type, args, kwargs);
    if (self == NULL) {
        free(conn);
        return NULL;
    }
    self->conn = conn;
    Debugp("accepted connection. conn=%p", (void*)self->conn);
    return (PyObject *)self;
}


PyDoc_STRVAR(Connection_accept_doc,
"accept(fd) -> connection\n\
\n\
Create a new connection from an fd open for listening.\n\
The function will perform a socket accept and once a\n\
connection is established return a new connection object.");


static PyObject *
Connection_close(MsgConnection *self, PyObject *args)
{
    Debugp("Closing connection: %p (on %p)",
           (void*)(self->conn), (void*)self);
    if (self->conn) {
        Debugp("Really closing %p", (void*)self->conn);
        /* close the conn object
           in an ideal world we'd check the return code */
        msg_close(self->conn);
        self->conn = NULL;
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(Connection_close_doc,
"close()\n\
\n\
Close the current connection.\n\
NOTE: May be subject to internal caching.");


static PyObject *
Connection_enter(MsgConnection *self, PyObject *args)
{
    if (self->conn == NULL) {
        return raise_closed();
    }
    Py_XINCREF(self);
    return (PyObject *)self;
}


PyDoc_STRVAR(Connection_enter_doc,
"__enter__() -> connection\n\
\n\
Enter a context manager.");


static PyObject *
Connection_exit(MsgConnection *self, PyObject *args)
{
    /* call python close method - works if subclassed */
    PyObject *ret = PyObject_CallMethod((PyObject*)self, "close", NULL);
    /* let errors pass thru */
    if (!ret)
        return NULL;
    Py_DECREF(ret);
    Py_RETURN_NONE;
}


PyDoc_STRVAR(Connection_exit_doc,
"__exit__(error, exc, tb)\n\
\n\
Exit the current context manager.\n\
Closes the current connection.");


static PyObject *
Connection_fileno(MsgConnection *self, PyObject *args)
{
    if (self->conn == NULL) {
        return raise_closed();
    } else {
        return Py_BuildValue("i", self->conn->fd);
    }
}

PyDoc_STRVAR(Connection_fileno_doc,
"fileno() -> integer\n\
\n\
Return the integer file descriptor. This can be passed to \
certain lower-level OS functions.");


static PyObject *
Connection_get_closed(MsgConnection *self, void *closure)
{
    return PyBool_FromLong((long)(self->conn == NULL));
}


static PyObject *
raise_closed(void)
{
    PyErr_SetString(PyExc_ValueError,
                    "operation on closed/unitialized connection");
    return NULL;
}


static PyMethodDef Connection_methods[] = {
    {"open", (PyCFunction)Connection_open,
              METH_VARARGS | METH_CLASS,
              Connection_open_doc},
    {"accept", (PyCFunction)Connection_accept,
              METH_VARARGS | METH_CLASS,
              Connection_accept_doc},
    {"close", (PyCFunction)Connection_close,
              METH_NOARGS,
              Connection_close_doc},
    {"fileno", (PyCFunction)Connection_fileno,
               METH_NOARGS,
               Connection_fileno_doc},
    {"__enter__", (PyCFunction)Connection_enter,
                  METH_NOARGS,
                  Connection_enter_doc},
    {"__exit__", (PyCFunction)Connection_exit,
                 METH_VARARGS,
                 Connection_exit_doc},
    {NULL}
};


static PyGetSetDef Connection_getset[] = {
    {"closed", (getter)Connection_get_closed, NULL, "connection is closed"},
    {NULL}
};


static PyTypeObject Connection_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "dispatch.Connection",     /*tp_name*/
    sizeof(MsgConnection),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Connection_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    "Connection",          /* tp_doc */
};


static int
Connection_type_setup(void) {
    Connection_type.tp_new = Connection_new;
    Connection_type.tp_methods = Connection_methods;
    Connection_type.tp_getset = Connection_getset;
    return PyType_Ready(&Connection_type);
}
/* END Connection Object */


#define GET_MSG_CONN(x) (((MsgConnection*)(x))->conn)


static int
open_connection(PyObject *arg, PyObject **dest)
{
    if (!arg) {
        return 0;
    }
    if (!PyObject_TypeCheck(arg, &Connection_type)) {
        PyErr_Format(PyExc_TypeError, "must be %s, not %s",
                     Connection_type.tp_name, arg->ob_type->tp_name);
        return 0;
    }
    if (!GET_MSG_CONN(arg)) {
        raise_closed();
        return 0;
    }
    Debugp("passed connection %p @ %p", GET_MSG_CONN(arg), arg);
    *dest = arg;
    return 1;
}


static inline PyObject *
write_result(int status)
{
    PyObject *obj;

    if (status < 0) {
        obj = PyErr_SetFromErrno(PyExc_IOError);
    } else {
        obj = Py_BuildValue("i", status);
    }
    return obj;
}


static inline PyObject *
read_result(int status, const char *fmt, ...)
{
    PyObject *obj;
    va_list argp;

    if (status < 0) {
        Debugp("status was less than zero (%d)", status);
        obj = PyErr_SetFromErrno(PyExc_IOError);
    } else if (status == 0) {
        Debugp("status was zero (no data)");
        errno = EPIPE;
        obj = PyErr_SetFromErrno(PyExc_IOError);
    } else {
        va_start(argp, fmt);
        obj = Py_VaBuildValue(fmt, argp);
        va_end(argp);
    }
    return obj;
}


static PyObject *
dispatch_msg_write_uint64(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int64_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&K", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_uint64(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_uint64_doc,
"dispatch_msg_write_uint64(conn, value) -> status : int\n\
\n\
Serialize the given value into a uint64 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_uint64(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint64_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_uint64(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "K", value);
}


PyDoc_STRVAR(dispatch_msg_read_uint64_doc,
"dispatch_msg_read_uint64(conn) -> int\n\
\n\
Read a serialized uint64 type value from the given connection.");


static PyObject *
dispatch_msg_write_int64(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int64_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&L", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_int64(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_int64_doc,
"dispatch_msg_write_int64(conn, value) -> status : int\n\
\n\
Serialize the given value into a int64 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_int64(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int64_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_int64(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "L", value);
}


PyDoc_STRVAR(dispatch_msg_read_int64_doc,
"dispatch_msg_read_int64(conn) -> int\n\
\n\
Read a serialized int64 type value from the given connection.");


static PyObject *
dispatch_msg_write_uint32(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int32_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&I", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_uint32(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_uint32_doc,
"dispatch_msg_write_uint32(conn, value) -> status : int\n\
\n\
Serialize the given value into a uint32 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_uint32(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint32_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_uint32(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "I", value);
}


PyDoc_STRVAR(dispatch_msg_read_uint32_doc,
"dispatch_msg_read_uint32(conn) -> int\n\
\n\
Read a serialized uint32 type value from the given connection.");


static PyObject *
dispatch_msg_write_int32(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int value;
    int status;

    if (!PyArg_ParseTuple(args, "O&i", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_int32(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_int32_doc,
"dispatch_msg_write_int32(conn, value) -> status : int\n\
\n\
Serialize the given value into a int32 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_int32(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int32_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_int32(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "i", value);
}


PyDoc_STRVAR(dispatch_msg_read_int32_doc,
"dispatch_msg_read_int32(conn) -> int\n\
\n\
Read a serialized int32 type value from the given connection.");


static PyObject *
dispatch_msg_write_uint16(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint16_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&H", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_uint16(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_uint16_doc,
"dispatch_msg_write_uint16(conn, value) -> status : int\n\
\n\
Serialize the given value into a uint16 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_uint16(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint16_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_uint16(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "H", value);
}


PyDoc_STRVAR(dispatch_msg_read_uint16_doc,
"dispatch_msg_read_uint16(conn) -> int\n\
\n\
Read a serialized uint16 type value from the given connection.");


static PyObject *
dispatch_msg_write_uint8(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint8_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&b", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_uint8(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_uint8_doc,
"dispatch_msg_write_uint8(conn, value) -> status : int\n\
\n\
Serialize the given value into a uint8 type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_uint8(PyObject *self, PyObject *args)
{
    PyObject *conn;
    uint8_t value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read value");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_uint8(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "b", value);
}


PyDoc_STRVAR(dispatch_msg_read_uint8_doc,
"dispatch_msg_read_uint8(conn) -> int\n\
\n\
Read a serialized uint8 type value from the given connection.");


static PyObject *
dispatch_msg_write_fd(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int value;
    int status;

    if (!PyArg_ParseTuple(args, "O&i", &open_connection, &conn, &value)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write fd");
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_fd(GET_MSG_CONN(conn), value);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_fd_doc,
"dispatch_msg_write_fd(conn, value) -> status : int\n\
\n\
Serialize the given fd value and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_fd(PyObject *self, PyObject *args)
{
    PyObject *conn;
    int value;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read fd");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_fd(GET_MSG_CONN(conn), &value);
    Py_END_ALLOW_THREADS
    return read_result(status, "i", value);
}


PyDoc_STRVAR(dispatch_msg_read_fd_doc,
"dispatch_msg_read_fd(conn) -> int\n\
\n\
Read a serialized fd from the given connection.");


static PyObject *
dispatch_msg_write_string(PyObject *self, PyObject *args)
{
    PyObject *conn;
    char *string = NULL;
    int status;

    if (!PyArg_ParseTuple(args, "O&s", &open_connection, &conn, &string)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Write string value=%s", string);
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_string(GET_MSG_CONN(conn), string);
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_string_doc,
"dispatch_msg_write_string(conn, value) -> status : int\n\
\n\
Serialize the given value into a string type and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_string(PyObject *self, PyObject *args)
{
    PyObject *conn;
    PyObject *out;
    char *string = NULL;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read string");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_string(GET_MSG_CONN(conn), &string);
    Py_END_ALLOW_THREADS
    Debugp("Read string value=%s", string);
    if (status >= 0 && string == NULL) {
        PyErr_SetString(PyExc_IOError, "unable to read valid string");
        out = NULL;
    } else {
        out = read_result(status, "s", string);
    }
    free(string);
    return out;
}


PyDoc_STRVAR(dispatch_msg_read_string_doc,
"dispatch_msg_read_string(conn) -> string\n\
\n\
Read a serialized string type value from the given connection.");


/*
 * NOTE: The msg_{read,write}_bytes functions wrap dispatch's
 * msg_{read,write}_buffer and msg_{read,write}_buffer_length
 * together in one set of functions.
 * This is Python and not doing them together makes no sense to me.
 * I also called this function "bytes" in order to avoid confusion
 * with the interface of the dispatch funcs and python's own
 * buffer types.
 *
 */


static PyObject *
dispatch_msg_write_bytes(PyObject *self, PyObject *args)
{
    PyObject *conn;
    char *bytes = NULL;
    int blength;
    int status;

    if (!PyArg_ParseTuple(args, "O&s#", &open_connection, &conn,
                                        &bytes, &blength)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("going to write %d bytes", blength);
    Py_BEGIN_ALLOW_THREADS
    status = msg_write_buffer_length(GET_MSG_CONN(conn), blength);
    if (status >= 0) {
        status = msg_write_buffer(GET_MSG_CONN(conn), bytes, blength);
    }
    Py_END_ALLOW_THREADS
    return write_result(status);
}


PyDoc_STRVAR(dispatch_msg_write_bytes_doc,
"dispatch_msg_write_bytes(conn, value) -> status : int\n\
\n\
Serialize the given byte string or buffer object and transmit\n\
it over the given connection.");


static PyObject *
dispatch_msg_read_bytes(PyObject *self, PyObject *args)
{
    PyObject *conn;
    PyObject *out;
    char *bytes = NULL;
    size_t blength;
    int status;

    if (!PyArg_ParseTuple(args, "O&", &open_connection, &conn)) {
        Debugp("invalid function arguments");
        return NULL;
    }
    Debugp("Read buffer/bytes");
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_buffer_length(GET_MSG_CONN(conn), &blength);
    Py_END_ALLOW_THREADS
    if (status < 0) {
        return PyErr_SetFromErrno(PyExc_IOError);
    }
    out = PyString_FromStringAndSize(NULL, blength);
    if (!out) {
        return NULL;
    }
    bytes = PyString_AsString(out);
    if (!bytes) {
        /* its possible for _AsString to return NULL, however I
         * don't know why it would here */
        Py_XDECREF(out);
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    status = msg_read_buffer(GET_MSG_CONN(conn), bytes, blength);
    Py_END_ALLOW_THREADS
    if (status < 0) {
        Py_XDECREF(out);
        return PyErr_SetFromErrno(PyExc_IOError);
    }
    return out;
}


PyDoc_STRVAR(dispatch_msg_read_bytes_doc,
"dispatch_msg_read_bytes(conn) -> bytes\n\
\n\
Read a serialized byte string type value from the given connection.");


static PyObject *
dispatch_listen_socket(PyObject *self, PyObject *args)
{
    int sock = -1;
    int err;
    char *host;
    char *service;
    int flags;

    if (!PyArg_ParseTuple(args, "ssi", &host, &service, &flags)) {
        return NULL;
    }
    if (strlen(host) == 0) {
        /* set empty string back to none for dispatch call */
        host = NULL;
    }
    Py_BEGIN_ALLOW_THREADS
    err = listen_socket(&sock, host, service, flags);
    Py_END_ALLOW_THREADS
    if (err != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Debugp("created listening socket");
    return Py_BuildValue("i", sock);
}


PyDoc_STRVAR(dispatch_listen_socket_doc,
"dispatch_listen_socket(host, service, flags) -> fd\n\
\n\
Return an open (socket) fd needed to listen to dispatch\n\
requests.");


static PyMethodDef Methods[] = {
    {"_listen_socket", dispatch_listen_socket,
     METH_VARARGS, dispatch_listen_socket_doc},
    {"msg_write_type", dispatch_msg_write_uint16,
     METH_VARARGS, dispatch_msg_write_uint16_doc},
    {"msg_read_type", dispatch_msg_read_uint16,
     METH_VARARGS, dispatch_msg_read_uint16_doc},
    {"msg_write_uint64", dispatch_msg_write_uint64,
     METH_VARARGS, dispatch_msg_write_uint64_doc},
    {"msg_read_uint64", dispatch_msg_read_uint64,
     METH_VARARGS, dispatch_msg_read_uint64_doc},
    {"msg_write_int64", dispatch_msg_write_int64,
     METH_VARARGS, dispatch_msg_write_int64_doc},
    {"msg_read_int64", dispatch_msg_read_int64,
     METH_VARARGS, dispatch_msg_read_int64_doc},
    {"msg_write_uint32", dispatch_msg_write_uint32,
     METH_VARARGS, dispatch_msg_write_uint32_doc},
    {"msg_read_uint32", dispatch_msg_read_uint32,
     METH_VARARGS, dispatch_msg_read_uint32_doc},
    {"msg_write_int32", dispatch_msg_write_int32,
     METH_VARARGS, dispatch_msg_write_int32_doc},
    {"msg_read_int32", dispatch_msg_read_int32,
     METH_VARARGS, dispatch_msg_read_int32_doc},
    {"msg_write_uint16", dispatch_msg_write_uint16,
     METH_VARARGS, dispatch_msg_write_uint16_doc},
    {"msg_read_uint16", dispatch_msg_read_uint16,
     METH_VARARGS, dispatch_msg_read_uint16_doc},
    {"msg_write_uint8", dispatch_msg_write_uint8,
     METH_VARARGS, dispatch_msg_write_uint8_doc},
    {"msg_read_uint8", dispatch_msg_read_uint8,
     METH_VARARGS, dispatch_msg_read_uint8_doc},
    {"msg_write_fd", dispatch_msg_write_fd,
     METH_VARARGS, dispatch_msg_write_fd_doc},
    {"msg_read_fd", dispatch_msg_read_fd,
     METH_VARARGS, dispatch_msg_read_fd_doc},
    {"msg_write_string", dispatch_msg_write_string,
     METH_VARARGS, dispatch_msg_write_string_doc},
    {"msg_read_string", dispatch_msg_read_string,
     METH_VARARGS, dispatch_msg_read_string_doc},
    {"msg_write_bytes", dispatch_msg_write_bytes,
     METH_VARARGS, dispatch_msg_write_bytes_doc},
    {"msg_read_bytes", dispatch_msg_read_bytes,
     METH_VARARGS, dispatch_msg_read_bytes_doc},

    {NULL, NULL, 0, NULL}
};


PyMODINIT_FUNC
init_dsdispatch(void)
{
    PyObject *mod;
    mod = Py_InitModule(MODULE_NAME, Methods);
    int res;

    /* flag constants */
    res = PyModule_AddIntConstant(mod, "MSG_LOCAL", MSG_LOCAL);
    if (res) return;
    res = PyModule_AddIntConstant(mod, "MSG_NORETURN", MSG_NORETURN);
    if (res) return;
    res = PyModule_AddIntConstant(mod, "MSG_NONBLOCK", MSG_NONBLOCK);
    if (res) return;

    if (Connection_type_setup() < 0) {
        return;
    }
    Py_INCREF(&Connection_type);
    PyModule_AddObject(mod, "Connection", (PyObject*)&Connection_type);
}
