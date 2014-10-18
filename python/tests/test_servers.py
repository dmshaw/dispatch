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
import sample_server_cli


def on_alarm(sig, frame):
    raise RuntimeError("caught alarm")


class ServerTestCase(unittest.TestCase):
    _server = None
    _wd = None

    @classmethod
    def setUpClass(cls):
        script, default_script, script_args = cls.SCRIPT
        suffix = cls.SUFFIX
        spath = os.environ.get(script, None)
        if not spath:
            spath = os.path.join(os.path.dirname(echo_server.__file__),
                                 default_script)
        cls._td = tempfile.mkdtemp(suffix=suffix)
        os.chdir(cls._td)
        outfh = open('output', 'wb')
        try:
            cls._server = subprocess.Popen(['python', spath, 'd.sock']
                                           + script_args,
                                           stdout=outfh,
                                           stderr=outfh)
        finally:
            outfh.close()
        # give server a moment to set up
        time.sleep(0.25)
        cls.spath = spath

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


class TestEchoServer(ServerTestCase):
    SCRIPT = ('ECHO_SERVER_SCRIPT', 'echo_server.py', [])
    SUFFIX = '.TestEchoServer'

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


class TestSampleServer(ServerTestCase):
    SCRIPT = ('SAMPLE_SERVER_SCRIPT', 'sample_server_cli.py', ['serve'])
    SUFFIX = '.TestSampleServer'

    def test_fact_internal(self):
        fname = os.path.join(self._td, 'd.sock')
        ss = sample_server_cli
        self.assert_server_alive()
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.COMP_FACT)
            dispatch.msg_write_int32(conn, 5)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            v = dispatch.msg_read_int32(conn)
            self.assertEqual(v, 120)
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.COMP_FACT)
            dispatch.msg_write_int32(conn, 10)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            v = dispatch.msg_read_int32(conn)
            self.assertEqual(v, 3628800)
        with dispatch.open('', fname) as conn:
            # the result will be too big
            dispatch.msg_write_type(conn, ss.COMP_FACT)
            dispatch.msg_write_int32(conn, 15)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_ERROR)
            v = dispatch.msg_read_uint16(conn)
            self.assertEqual(v, 1)
        with dispatch.open('', fname) as conn:
            # negative inputs are invalid
            dispatch.msg_write_type(conn, ss.COMP_FACT)
            dispatch.msg_write_int32(conn, -7)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_ERROR)
            v = dispatch.msg_read_uint16(conn)
            self.assertEqual(v, 1)

    def test_fact_external(self):
        p = subprocess.Popen(['python', self.spath, 'd.sock', 'fact', '7'],
                             stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.assertEqual(p.returncode, 0)
        self.assertEqual(out.strip(), '5040')

        p = subprocess.Popen(['python', self.spath, 'd.sock', 'fact', '11'],
                             stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.assertEqual(p.returncode, 0)
        self.assertEqual(out.strip(), '39916800')

        p = subprocess.Popen(['python', self.spath, 'd.sock', 'fact', '30'],
                             stdout=subprocess.PIPE)
        out, err = p.communicate()
        self.assertEqual(p.returncode, 1)

    def test_open_rand_source(self):
        fname = os.path.join(self._td, 'd.sock')
        ss = sample_server_cli
        self.assert_server_alive()
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.OPEN_RAND)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            fd = dispatch.msg_read_fd(conn)
            self.assertTrue(fd > 0)
            try:
                buf = os.read(fd, 8)
                self.assertEqual(len(buf), 8)
            finally:
                os.close(fd)
            self.assertRaises((OSError, IOError), os.read, fd, 8)

    def test_round_trip_reverse(self):
        fname = os.path.join(self._td, 'd.sock')
        ss = sample_server_cli
        self.assert_server_alive()
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.ROUND_TRIP)
            dispatch.msg_write_type(conn, 2500)
            dispatch.msg_write_uint64(conn, 18446743967598447505)
            dispatch.msg_write_int64(conn, -9223372036854775807)
            dispatch.msg_write_uint32(conn, 2147477648)
            dispatch.msg_write_int32(conn, -6000)
            dispatch.msg_write_uint16(conn, 512)
            dispatch.msg_write_uint8(conn, 255)
            dispatch.msg_write_string(conn, "Hello Earth!")
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            values = []
            values.append(dispatch.msg_read_string(conn))
            values.append(dispatch.msg_read_uint8(conn))
            values.append(dispatch.msg_read_uint16(conn))
            values.append(dispatch.msg_read_int32(conn))
            values.append(dispatch.msg_read_uint32(conn))
            values.append(dispatch.msg_read_int64(conn))
            values.append(dispatch.msg_read_uint64(conn))
            values.append(dispatch.msg_read_type(conn))
            self.assertEqual(values, [
                "Hello Earth!",
                255, 512,
                -6000, 2147477648,
                -9223372036854775807, 18446743967598447505,
                2500])

    def test_bytes_type(self):
        import hashlib
        d1 = b'my\0binary\0data\0'
        d2 = hashlib.sha1('Goodbye Moon').digest()
        d3 = b'\x00\x00\x08abc'
        fname = os.path.join(self._td, 'd.sock')
        ss = sample_server_cli
        self.assert_server_alive()
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.A_FEW_BYTES)
            dispatch.msg_write_bytes(conn, d1)
            dispatch.msg_write_bytes(conn, d2)
            dispatch.msg_write_bytes(conn, d3)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            r0 = dispatch.msg_read_uint32(conn)
            self.assertEqual(r0,
                len(d1) + len(d2) + len(d3))
            r1 = dispatch.msg_read_bytes(conn)
            r2 = dispatch.msg_read_bytes(conn)
            r3 = dispatch.msg_read_bytes(conn)
            self.assertEqual(r1, d1)
            self.assertEqual(r2, d2)
            self.assertEqual(r3, d3)

    def test_eof_reads(self):
        fname = os.path.join(self._td, 'd.sock')
        ss = sample_server_cli
        self.assert_server_alive()
        with dispatch.open('', fname) as conn:
            dispatch.msg_write_type(conn, ss.COMP_FACT)
            dispatch.msg_write_int32(conn, 5)
            t = dispatch.msg_read_type(conn)
            self.assertEqual(t, ss.T_RESULT)
            v = dispatch.msg_read_int32(conn)
            self.assertEqual(v, 120)
            self.assertRaises(IOError, dispatch.msg_read_uint16, conn)
            self.assertRaises(IOError, dispatch.msg_read_uint32, conn)
            self.assertRaises(IOError, dispatch.msg_read_int32, conn)


if __name__ == '__main__':
    unittest.main()
