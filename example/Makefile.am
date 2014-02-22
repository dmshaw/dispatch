noinst_PROGRAMS=server client

server_SOURCES=server.c common.h
server_CPPFLAGS=-I$(top_builddir)/include
server_LDADD=$(top_builddir)/lib/libdispatch.la

client_SOURCES=client.c common.h
client_CPPFLAGS=-I$(top_builddir)/include
client_LDADD=$(top_builddir)/lib/libdispatch.la
