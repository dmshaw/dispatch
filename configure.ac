#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.63)
AC_INIT([dispatch],[0.7next],[dshaw@jabberwocky.com])
AC_CONFIG_SRCDIR([lib/msg.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([std-options])

# Checks for programs.
AC_PROG_CC
LT_INIT

# Checks for libraries.
AC_MSG_CHECKING([for pthreads])
_save_libs=$LIBS
LIBS="$_save_libs -lpthread"
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>],[pthread_cleanup_push(0,0); pthread_cleanup_pop(0);])],found=yes,found=no)
AC_MSG_RESULT($found)
if test $found = no ; then
   AC_MSG_ERROR([dispatch requires pthreads])
fi
LIBS=$_save_libs

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

dnl # TCP_CORK is a real help, but we can work without it.
dnl AC_CHECK_DECLS([TCP_CORK],,,[#include <netinet/tcp.h>])

# Checks for library functions.
AC_CHECK_FUNCS([syslog])

AC_ARG_ENABLE(optimization,
   AS_HELP_STRING([--disable-optimization],[disable compiler optimization]),
   [if test $enableval = no ; then
      CFLAGS=`echo $CFLAGS | sed 's/-O[[0-9]]//'`
   fi])

if test "$GCC" = yes; then
   CFLAGS="-Wall -Werror -pedantic $CFLAGS"
fi

AC_CONFIG_FILES([Makefile lib/Makefile example/Makefile dispatch.pc dispatch.spec])
AC_OUTPUT
