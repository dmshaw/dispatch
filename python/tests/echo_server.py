#!/usr/bin/env python2
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

import dsdispatch as dispatch
import os

ECHO_STRING = 2
ECHO_INT = 3
ECHO_HELLO = 4


def echo_string(dtype, conn):
    print 'EString 01'
    s1 = dispatch.msg_read_string(conn)
    print 'EString 02', s1
    dispatch.msg_write_string(conn, s1)
    print 'EString 03'
    dispatch.msg_write_uint16(conn, 0)
    print 'EString 04'


def echo_int(dtype, conn):
    i1 = dispatch.msg_read_int32(conn)
    dispatch.msg_write_int32(conn, i1)
    dispatch.msg_write_uint16(conn, 0)


def echo_hello(dtype, conn):
    dispatch.msg_write_string(conn, "Hello, World")
    dispatch.msg_write_uint16(conn, 0)


def main(args):
    service = os.path.abspath(args[0])
    dispatch.msg_listen("", service, {
        ECHO_STRING: echo_string,
        ECHO_INT: echo_int,
        ECHO_HELLO: echo_hello,
    })


if __name__ == '__main__':
    import sys
    main(sys.argv[1:])
