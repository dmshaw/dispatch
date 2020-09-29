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
import math
import os
import sys

T_ERROR = 8
T_RESULT = 9
COMP_FACT = 10
OPEN_RAND = 11
ROUND_TRIP = 12
A_FEW_BYTES = 13
OPEN_ZERO = 14


def comp_fact(dtype, conn):
    try:
        v = dispatch.msg_read_int32(conn)
        res = compute_fact(v)
        dispatch.msg_write_type(conn, T_RESULT)
        dispatch.msg_write_int32(conn, res)
    except (IOError, OSError, ValueError):
        dispatch.msg_write_type(conn, T_ERROR)
        dispatch.msg_write_uint16(conn, 1)


def open_rand(dtype, conn):
    fd = None
    try:
        fd = os.open('/dev/urandom', os.O_RDONLY)
        dispatch.msg_write_type(conn, T_RESULT)
        dispatch.msg_write_fd(conn, fd)
    except (IOError, OSError, ValueError):
        dispatch.msg_write_type(conn, T_ERROR)
        dispatch.msg_write_uint16(conn, 1)
    finally:
        if fd:
            os.close(fd)


def open_zero(dtype, conn):
    with open('/dev/zero', 'r+b') as fh:
        try:
            dispatch.msg_write_type(conn, T_RESULT)
            dispatch.msg_write_file(conn, fh)
        except (IOError, OSError, ValueError):
            dispatch.msg_write_type(conn, T_ERROR)
            dispatch.msg_write_uint16(conn, 1)


UINT32_MAX = (1 << 31) - 1


def compute_fact(value):
    res = math.factorial(value)
    if res > UINT32_MAX:
        raise ValueError('bad fit')
    return res


def rq_fact(sockfn, value):
    with dispatch.open('', sockfn) as conn:
        dispatch.msg_write_type(conn, COMP_FACT)
        dispatch.msg_write_int32(conn, value)
        t = dispatch.msg_read_type(conn)
        if t == T_ERROR:
            e = dispatch.msg_read_uint16(conn)
            sys.exit(e)
        v = dispatch.msg_read_int32(conn)
    print v


def round_trip_reverse(dtype, conn):
    try:
        values = []
        values.append(dispatch.msg_read_type(conn))
        values.append(dispatch.msg_read_uint64(conn))
        values.append(dispatch.msg_read_int64(conn))
        values.append(dispatch.msg_read_uint32(conn))
        values.append(dispatch.msg_read_int32(conn))
        values.append(dispatch.msg_read_uint16(conn))
        values.append(dispatch.msg_read_uint8(conn))
        values.append(dispatch.msg_read_string(conn))
        print >>sys.stderr, 'values=%r' % values
        dispatch.msg_write_type(conn, T_RESULT)
        # now the data:
        dispatch.msg_write_string(conn, values.pop())
        dispatch.msg_write_uint8(conn, values.pop())
        dispatch.msg_write_uint16(conn, values.pop())
        dispatch.msg_write_int32(conn, values.pop())
        dispatch.msg_write_uint32(conn, values.pop())
        dispatch.msg_write_int64(conn, values.pop())
        dispatch.msg_write_uint64(conn, values.pop())
        dispatch.msg_write_type(conn, values.pop())
    except Exception:
        dispatch.msg_write_type(conn, T_ERROR)
        dispatch.msg_write_uint16(conn, 1)


def a_few_bytes(dtype, conn):
    try:
        d1 = dispatch.msg_read_bytes(conn)
        print >>sys.stderr, 'Got bytes: %r' % d1
        d2 = dispatch.msg_read_bytes(conn)
        print >>sys.stderr, 'Got bytes: %r' % d2
        d3 = dispatch.msg_read_bytes(conn)
        print >>sys.stderr, 'Got bytes: %r' % d3
        total_len = len(d1) + len(d2) + len(d3)
        dispatch.msg_write_type(conn, T_RESULT)
        dispatch.msg_write_uint32(conn, total_len)
        dispatch.msg_write_bytes(conn, d1)
        dispatch.msg_write_bytes(conn, d2)
        dispatch.msg_write_bytes(conn, d3)
    except Exception:
        dispatch.msg_write_type(conn, T_ERROR)
        dispatch.msg_write_uint16(conn, 1)


def main(args):
    sockfn = os.path.abspath(args[0])
    if args[1] == 'serve':
        serve(sockfn)
        return
    if args[1] == 'fact':
        rq_fact(sockfn, int(args[2]))
        return
    raise ValueError('bad action: %s' % args[1])


def serve(sockfn):
    dispatch.msg_listen("", sockfn, {
        COMP_FACT: comp_fact,
        OPEN_RAND: open_rand,
        OPEN_ZERO: open_zero,
        ROUND_TRIP: round_trip_reverse,
        A_FEW_BYTES: a_few_bytes,
    })


if __name__ == '__main__':
    import sys
    main(sys.argv[1:])
