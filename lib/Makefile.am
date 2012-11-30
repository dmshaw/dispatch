#INCLUDES=-I${top_srcdir}/include
#bin_PROGRAMS=tester sender
#tester_LDADD=libdispatch.la
#sender_LDADD=libdispatch.la

lib_LTLIBRARIES=libdispatch.la

libdispatch_la_SOURCES=msg.c conn.c conn.h dispatch.c types.c
libdispatch_la_CPPFLAGS=-I$(top_srcdir)/include
libdispatch_la_LIBADD=-lpthread

# This flag accepts an argument of the form
# current[:revision[:age]]. So, passing -version-info 3:12:1 sets
# current to 3, revision to 12, and age to 1.
#
# If either revision or age are omitted, they default to 0. Also note
# that age must be less than or equal to the current interface number.
#
# Here are a set of rules to help you update your library version
# information:
#
# 1. Start with version information of `0:0:0' for each libtool
# library.
#
# 2. Update the version information only immediately before a public
# release of your software. More frequent updates are unnecessary, and
# only guarantee that the current interface number gets larger faster.
#
# 3. If the library source code has changed at all since the last
# update, then increment revision (`c:r:a' becomes `c:r+1:a').
#
# 4. If any interfaces have been added, removed, or changed since the
# last update, increment current, and set revision to 0.
#
# 5. If any interfaces have been added since the last public release,
# then increment age.
#
# 6. If any interfaces have been removed since the last public
# release, then set age to 0.

libdispatch_la_LDFLAGS=-version-info 0:0:0
