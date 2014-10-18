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
import echo_server


def on_alarm(sig, frame):
    raise RuntimeError("caught alarm")



class TestEchoServer(unittest.TestCase):
    _server = None
    _wd = None

    @classmethod
    def setUpClass(cls):
        spath = os.environ.get('SERVER_SCRIPT', None)
        if not spath:
            spath = os.path.join(os.path.dirname(echo_server.__file__),
                                 'echo_server.py')
        cls._td = tempfile.mkdtemp(suffix='.TestEchoServer')
        os.chdir(cls._td)
        cls._server = subprocess.Popen(['python', spath, 'd.sock'])
        # give server a moment to set up
        time.sleep(0.25)

    @classmethod
    def tearDownClass(cls):
        os.kill(cls._server.pid, signal.SIGTERM)
        cls._server.wait()
        os.chdir('..')
        if os.environ.get("TEST_NO_REMOVE") != "1":
            shutil.rmtree(cls._td)

    def setUp(self):
        signal.signal(signal.SIGALRM, on_alarm)
        signal.alarm(120)

    def tearDown(self):
        signal.signal(signal.SIGALRM, signal.SIG_IGN)

    def assert_server_alive(self):
        pid = self._server.pid
        try:
            os.kill(pid, 0)
        except (IOError, OSError):
            raise AssertionError('server not alive')

    def test_foobar(self):
        self.assert_server_alive()
        fname = os.path.join(self._td, 'd.sock')
        conn = dispatch.open('', fname)
        with conn:
            dispatch.msg_write_type(conn, echo_server.ECHO_STRING)
            dispatch.msg_write_string(conn, "foobar")

            s = dispatch.msg_read_string(conn)
            # mimic the common error number style
            e = dispatch.msg_read_uint16(conn)
            self.assertEqual(s, 'foobar')
            self.assertEqual(e, 0)

    def test_hello(self):
        self.assert_server_alive()
        fname = os.path.join(self._td, 'd.sock')
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, echo_server.ECHO_HELLO)
            s = dispatch.msg_read_string(conn)
            e = dispatch.msg_read_uint16(conn)
        self.assertEqual(s, "Hello, World")
        self.assertEqual(e, 0)

    def test_hello_multi(self):
        fname = os.path.join(self._td, 'd.sock')
        for ii in range(0, 5):
            self.assert_server_alive()
            with dispatch.open('', fname) as conn:
                dispatch.msg_write_type(conn, echo_server.ECHO_HELLO)
                s = dispatch.msg_read_string(conn)
                e = dispatch.msg_read_uint16(conn)
            self.assertEqual(s, "Hello, World")
            self.assertEqual(e, 0)

    def test_multiple_rounds(self):
        self.assert_server_alive()
        fname = os.path.join(self._td, 'd.sock')
        for ii in range(0, 16):
            self.assert_server_alive()
            with dispatch.open('', fname) as conn:
                tostr = chr(0x41 + ii) * (ii + 1)
                dispatch.msg_write_type(conn, echo_server.ECHO_STRING)
                dispatch.msg_write_string(conn, tostr)
                fromstr = dispatch.msg_read_string(conn)
                err = dispatch.msg_read_uint16(conn)
            self.assertEqual(err, 0)
            self.assertEqual(tostr, fromstr)


if __name__ == '__main__':
    unittest.main()
