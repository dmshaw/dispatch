ACLOCAL_AMFLAGS=-I m4
SUBDIRS=lib example
if PYTHON
   SUBDIRS+=python
endif
include_HEADERS=include/dispatch.h
pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=dispatch.pc
EXTRA_DIST=dispatch.spec
