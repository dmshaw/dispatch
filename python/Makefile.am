AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/lib -std=c99

pyexec_LTLIBRARIES=_dsdispatch.la
_dsdispatch_la_LDFLAGS=-module -shared -avoid-version
_dsdispatch_la_CPPFLAGS=$(AM_CPPFLAGS) @python_CFLAGS@ -fno-strict-aliasing
_dsdispatch_la_SOURCES=_dsdispatch.c
_dsdispatch_la_LIBADD = ../lib/libdispatch.la

dsdispatchdir=$(pyexecdir)
dsdispatch_PYTHON=dsdispatch.py

TESTS=$(top_builddir)/python/tests/runtests.py
EXTRA_DIST=$(TESTS) $(top_builddir)/python/tests/echo_server.py $(top_builddir)/python/tests/test_echo_server.py $(top_builddir)/python/tests/runtests.py $(top_builddir)/python/tests/test_servers.py $(top_builddir)/python/tests/sample_server_cli.py $(top_builddir)/python/tests/test_threaded.py
