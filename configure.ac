AC_INIT(ev_epoll.c)

AM_INIT_AUTOMAKE(libev,1.85)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

if test "x$GCC" = xyes ; then
  CFLAGS="$CFLAGS -O3"
fi

m4_include([libev.m4])

AC_OUTPUT(Makefile)
