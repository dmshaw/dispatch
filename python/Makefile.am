AM_CPPFLAGS=-I$(top_builddir)/include -I$(top_builddir)/lib -std=c99

pyexec_LTLIBRARIES=_dsdispatch.la
_dsdispatch_la_LDFLAGS=-module -shared -avoid-version
_dsdispatch_la_CPPFLAGS=$(AM_CPPFLAGS) @python_CFLAGS@ -fno-strict-aliasing
_dsdispatch_la_SOURCES=_dsdispatch.c
_dsdispatch_la_LIBADD = ../lib/libdispatch.la

dsdispatchdir=$(pyexecdir)
dsdispatch_PYTHON=dsdispatch.py

TESTS=$(top_builddir)/python/tests/runtests.py
EXTRA_DIST=$(top_builddir)/python/tests/*.py
