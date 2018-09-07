AC_PREREQ([2.69])
AC_INIT([libespif], [0.0], [])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIRS([m4])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE

AC_PROG_CC
AM_PROG_AR
AC_ENABLE_SHARED
LT_INIT

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CHECK_HEADERS([stdio.h stdlib.h unistd.h string.h sys/types.h sys/socket.h errno.h netinet/in.h netinet/tcp.h netdb.h poll.h getopt.h stdbool.h time.h])
AC_CHECK_HEADER_STDBOOL

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_CHECK_FUNCS([socket])

AC_OUTPUT
