dnl this file is part of libev, do not make local modifications
dnl http://software.schmorp.de/pkg/libev

dnl libev support 
AC_CHECK_HEADERS(sys/epoll.h sys/event.h sys/queue.h port.h) 
 
AC_CHECK_FUNCS(epoll_ctl kqueue port_create)
 
AC_CHECK_FUNC(clock_gettime, [], [ 
   if test -z "$LIBEV_M4_AVOID_LIBRT"; then
     AC_CHECK_LIB(rt, clock_gettime) 
     AC_CHECK_FUNCS(clock_gettime)
   fi
])

AC_CHECK_LIB(m, ceil)



