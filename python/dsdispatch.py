#!/usr/bin/env python
#
# Python language wrapper for low-level dispatch functions
#
# Copyright (c) 2014  John Mulligan <johnm@asynchrono.us>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#
"""
Provides a more pythonic api around the dispatch C api functions.
Implements functions for using dispatch in a threaded server.

It is indended that as much of the c-api seem familiar as possible
but without violating the typical conventions of Python.

== Client Example ==

>>> import dsdispatch as dispatch
>>> conn = dispatch.open("", "/tmp/myapp.sock")
>>> dispatch.msg_write_type(conn, 22)
>>> dispatch.msg_write_string(conn, "foobar")
>>> t = dispatch.msg_read_type(conn)
>>> if t == 0:
...     print 'success', dispatch.msg_read_string(conn)
... else:
...     print 'error code=%d' % dispatch.msg_read_int32(conn)


== Simple Server Example ==

>>> import dsdispatch as dispatch
>>> THING1, THING2 = 1, 2
>>> def do_thing1(dtype, conn):
...     v = dispatch.msg_read_int32(conn)
...     dispatch.msg_write_int32(conn, v + 1)
...     return
>>> def do_thing2(dtype, conn):
...     v = dispatch.msg_read_int32(conn)
...     dispatch.msg_write_int32(conn, v + 2)
...     return
>>> dispatch.msg_listen("", service, {
...     THING1: do_thing1,
...     THING2: do_thing2,
... })

"""

import os
import errno
import threading

from _dsdispatch import \
    MSG_LOCAL, \
    _listen_socket, \
    msg_write_type, \
    msg_read_type, \
    msg_write_uint64, \
    msg_read_uint64, \
    msg_write_int64, \
    msg_read_int64, \
    msg_write_uint32, \
    msg_read_uint32, \
    msg_write_int32, \
    msg_read_int32, \
    msg_write_uint16, \
    msg_read_uint16, \
    msg_write_uint8, \
    msg_read_uint8, \
    msg_write_fd, \
    msg_read_fd, \
    msg_write_string, \
    msg_read_string, \
    msg_write_bytes, \
    msg_read_bytes, \
    Connection


__all__ = [
    # c wrapper module
    'Connection',
    'MSG_LOCAL',
    'msg_write_type',
    'msg_read_type',
    'msg_write_uint64',
    'msg_read_uint64',
    'msg_write_int64',
    'msg_read_int64',
    'msg_write_uint32',
    'msg_read_uint32',
    'msg_write_int32',
    'msg_read_int32',
    'msg_write_uint16',
    'msg_read_uint16',
    'msg_write_uint8',
    'msg_read_uint8',
    'msg_write_fd',
    'msg_read_fd',
    'msg_write_string',
    'msg_read_string',
    'msg_write_bytes',
    'msg_read_bytes',
    # this module
    'open',
    'Dispatcher',
    'msg_listen',
]


def open(host, service, flags=None):
    """Convenience wrapper for Connection.open"""
    if not flags:
        flags = MSG_LOCAL
    return Connection.open(host, service, flags)


class Dispatcher(object):
    """Implements a stateful system for dispatching messages
    to handlers in threads.
    """
    max_threads = 128
    daemon_threads = True
    def __init__(self, handlers):
        self.handlers = handlers
        self.sem = threading.Semaphore(self.max_threads)
        self.active = threading.Event()
        self.timeout = None

    def serve(self, host, service, flags=None):
        """Begin serving dispatch connections."""
        if not flags:
            flags = MSG_LOCAL
        sockfd = _listen_socket(host, service, flags)
        self.active.set()
        try:
            while self.active.is_set():
                if not self.ready(sockfd):
                    continue
                self.sem.acquire()
                conn = Connection.accept(sockfd)
                typeval, handler = self.gethandler(conn)
                if handler:
                    self.dispatch(conn, typeval, handler)
        finally:
            os.close(sockfd)

    def ready(self, sockfd):
        if not self.timeout:
            return True
        import select
        rl, wl, xl = select.select([sockfd], [], [sockfd], self.timeout)
        return (rl or xl)

    def gethandler(self, conn):
        """Return the handler function apropriate for the newly
        established connection."""
        try:
            return _handle_connection(conn, self.handlers)
        except (IOError, OSError) as read_error:
            if read_error.errno == errno.EPIPE:
                # client aborted, no reason to kill main loop
                return None, None
            else:
                raise

    def dispatch(self, conn, typeval, handler):
        """Create a new thread and execute message handling function"""
        t = threading.Thread(target=self.handle,
                             args=[conn, typeval, handler])
        t.daemon = self.daemon_threads
        t.start()

    def handle(self, conn, typeval, handler):
        """Run the handler function"""
        try:
            handler(typeval, conn)
        finally:
            self.sem.release()
            conn.close()


def msg_listen(host, service, handlers, flags=None):
    """Listen to a socket on the given host and service and
    execute given handlers in the style of dispatch's msg_listen.
    """
    # mimic dispatch c-library msg_listen function
    Dispatcher(handlers).serve(host, service, flags=flags)


def _read_header(conn):
    # normal dispatch lib writes these values but never
    # appears to check them
    hdr = (msg_read_uint8(conn), msg_read_uint8(conn))
    return hdr


def _handle_connection(conn, handlers):
    _read_header(conn)
    typeval = msg_read_type(conn)
    if typeval in handlers:
        handler = handlers[typeval]
    else:
        raise ValueError('unknown type value: %d' % typeval)
    return (typeval, handler)


def msg_write_file(conn, fileobj):
    """Pass the file descriptor associated with fileobj over the connection.
    """
    if getattr(fileobj, 'fileno'):
        fd = fileobj.fileno()
    else:
        raise ValueError(
            'fileobj has no fileno method (use msg_*_fd for fd ints)')
    return msg_write_fd(conn, fd)


def msg_read_file(conn, mode=None):
    """Return a file object (associated with a file descriptor) passed over
    the dispatch connection. If mode is specified the returned file object
    will be created with that mode, otherwise r+b is used as the default.
    """
    fd = msg_read_fd(conn)
    return os.fdopen(fd, (mode or 'r+b'))
