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

import os
import sys
import subprocess


def rtest(cmd):
    sys.stdout.write('$ %s\n' % cmd)
    ret = subprocess.call(cmd, shell=True)
    if ret:
        sys.exit(ret)

# set up the environment needed for the tests
pp = os.environ.get('PYTHONPATH', '')
wd = os.path.abspath('.')
if pp:
    pp += ':'
pp += ":".join([
    wd,
    os.path.join(wd, os.environ.get('srcdir', '')),
    os.path.join(wd, os.environ.get('srcdir', ''), 'tests'),
    os.path.join(wd, '.libs'),
])
os.environ['PYTHONPATH'] = pp

if len(sys.argv) > 1:
    for tspec in sys.argv[1:]:
        tspec = tspec.replace('+', 'python python/tests/')
        rtest(tspec)
else:
    rtest('python2 -c "import dsdispatch"')
    rtest('python2 ' + os.environ.get('srcdir', '') + '/tests/test_threaded.py')
    rtest('python2 ' + os.environ.get('srcdir', '') + '/tests/test_servers.py')
