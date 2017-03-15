AM_CPPFLAGS=-I$(top_srcdir)/include
noinst_PROGRAMS=server client

server_SOURCES=server.c common.h
server_LDADD=$(top_builddir)/lib/libdispatch.la

client_SOURCES=client.c common.h
client_LDADD=$(top_builddir)/lib/libdispatch.la
