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

try:
    import unittest2 as unittest
except ImportError:
    import unittest
import os
import tempfile
import shutil
import subprocess
import signal
import time

import dsdispatch as dispatch
import threading


def on_alarm(sig, frame):
    raise RuntimeError("caught alarm")


class TestCase(unittest.TestCase):
    TEMP = None
    SERVE = True
    SOCKF = None

    @classmethod
    def setUpClass(cls):
        suffix = cls.__name__
        cls.TEMP = tempfile.mkdtemp(suffix=suffix)
        os.chdir(cls.TEMP)
        if not cls.SERVE:
            return
        cls._t = threading.Thread(target=cls._start_server)
        cls._t.start()

    @classmethod
    def tearDownClass(cls):
        if os.environ.get("TEST_NO_REMOVE") != "1":
            shutil.rmtree(cls.TEMP)
        if not cls.SERVE:
            return
        cls._stop_server()

    def setUp(self):
        signal.signal(signal.SIGALRM, on_alarm)
        signal.alarm(120)

    def tearDown(self):
        signal.signal(signal.SIGALRM, signal.SIG_IGN)

    @classmethod
    def _start_server(cls):
        cls.SOCKF = os.path.join(cls.TEMP, 'd.sock')
        h = cls.server_handlers()
        cls._d = dispatch.Dispatcher(h)
        cls._d.timeout = 1
        cls._d.serve('', cls.SOCKF)

    @classmethod
    def _stop_server(cls):
        cls._d.active.clear()

MSG_REPLY = 8
MSG_ECHO = 10
MSG_REVERSE = 11


def handle_echo(dtype, conn):
    v1 = dispatch.msg_read_string(conn)
    v2 = dispatch.msg_read_int64(conn)
    v3 = dispatch.msg_read_uint8(conn)
    dispatch.msg_write_type(conn, MSG_REPLY)
    dispatch.msg_write_string(conn, v1)
    dispatch.msg_write_int64(conn, v2)
    dispatch.msg_write_uint8(conn, v3)
    return


def handle_reverse(dtype, conn):
    v1 = dispatch.msg_read_string(conn)
    v2 = dispatch.msg_read_int64(conn)
    v3 = dispatch.msg_read_uint8(conn)
    dispatch.msg_write_type(conn, MSG_REPLY)
    dispatch.msg_write_uint8(conn, v3)
    dispatch.msg_write_int64(conn, v2)
    dispatch.msg_write_string(conn, ''.join(reversed(v1)))
    return


class EchoTestCase(TestCase):
    @classmethod
    def server_handlers(self):
        return {
            MSG_ECHO: handle_echo,
            MSG_REVERSE: handle_reverse,
        }

    def test_open_close(self):
        conn = dispatch.open('', self.SOCKF)
        self.assertFalse(conn.closed)
        self.assertIsInstance(conn.fileno(), int)
        self.assertTrue(conn.fileno() > 0)
        conn.close()
        self.assertTrue(conn.closed)
        self.assertRaises(ValueError, conn.fileno)

    def text_ctx_manager(self):
        conn = dispatch.open('', self.SOCKF)
        self.assertFalse(conn.closed)
        with conn:
            self.assertFalse(conn.closed)
            self.assertTrue(conn.fileno() > 0)
        self.assertTrue(conn.closed)
        try:
            with conn:
                conn.fileno()
            failed2 = False
        except ValueError:
            failed2 = True
        self.assertTrue(failed2)

    def test_single_operation(self):
        with dispatch.open('', self.SOCKF) as conn:
            dispatch.msg_write_type(conn, MSG_ECHO)
            dispatch.msg_write_string(conn, 'Hello World')
            dispatch.msg_write_int64(conn, 5050505)
            dispatch.msg_write_uint8(conn, 0xA)
            self.assertEqual(dispatch.msg_read_type(conn), MSG_REPLY)
            r1 = dispatch.msg_read_string(conn)
            r2 = dispatch.msg_read_int64(conn)
            r3 = dispatch.msg_read_uint8(conn)
            self.assertEqual(r1, 'Hello World')
            self.assertEqual(r2, 5050505)
            self.assertEqual(r3, 0xA)

    def test_single_operation2(self):
        with dispatch.open('', self.SOCKF) as conn:
            dispatch.msg_write_type(conn, MSG_ECHO)
            dispatch.msg_write_string(conn, 'Hello World')
            dispatch.msg_write_int64(conn, 5050505)
            dispatch.msg_write_uint8(conn, 0xA)
            self.assertEqual(dispatch.msg_read_type(conn), MSG_REPLY)
            r1 = dispatch.msg_read_string(conn)
            r2 = dispatch.msg_read_int64(conn)
            r3 = dispatch.msg_read_uint8(conn)
            self.assertEqual(r1, 'Hello World')
            self.assertEqual(r2, 5050505)
            self.assertEqual(r3, 0xA)

    def test_triple_operation(self):
        with dispatch.open('', self.SOCKF) as conn:
            dispatch.msg_write_type(conn, MSG_ECHO)
            dispatch.msg_write_string(conn, 'Hello World')
            dispatch.msg_write_int64(conn, 5050505)
            dispatch.msg_write_uint8(conn, 0xA)
            self.assertEqual(dispatch.msg_read_type(conn), MSG_REPLY)
            r1 = dispatch.msg_read_string(conn)
            r2 = dispatch.msg_read_int64(conn)
            r3 = dispatch.msg_read_uint8(conn)
            self.assertEqual(r1, 'Hello World')
            self.assertEqual(r2, 5050505)
            self.assertEqual(r3, 0xA)
        with dispatch.open('', self.SOCKF) as conn:
            dispatch.msg_write_type(conn, MSG_ECHO)
            dispatch.msg_write_string(conn, 'Hello Earth')
            dispatch.msg_write_int64(conn, 5050505)
            dispatch.msg_write_uint8(conn, 0xA)
            self.assertEqual(dispatch.msg_read_type(conn), MSG_REPLY)
            r1 = dispatch.msg_read_string(conn)
            r2 = dispatch.msg_read_int64(conn)
            r3 = dispatch.msg_read_uint8(conn)
            self.assertEqual(r1, 'Hello Earth')
            self.assertEqual(r2, 5050505)
            self.assertEqual(r3, 0xA)
        with dispatch.open('', self.SOCKF) as conn:
            dispatch.msg_write_type(conn, MSG_ECHO)
            dispatch.msg_write_string(conn, 'Hello Mars')
            dispatch.msg_write_int64(conn, 5050505)
            dispatch.msg_write_uint8(conn, 0xA)
            self.assertEqual(dispatch.msg_read_type(conn), MSG_REPLY)
            r1 = dispatch.msg_read_string(conn)
            r2 = dispatch.msg_read_int64(conn)
            r3 = dispatch.msg_read_uint8(conn)
            self.assertEqual(r1, 'Hello Mars')
            self.assertEqual(r2, 5050505)
            self.assertEqual(r3, 0xA)

if __name__ == '__main__':
    unittest.main()
