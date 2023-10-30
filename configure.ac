AC_PREREQ([2.71])
AC_INIT([lwip-lowpower],[0.1],[virusv@live.com],[lwip-lowpower])
AC_CONFIG_SRCDIR([lwip-lowpower.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([tools])
AC_ARG_ENABLE([debug],[AS_HELP_STRING([--enable-debug],
              [enable debug options])],
              [CPPFLAGS="-DLWIP_DEBUG=1 $CPPFLAGS"])
AC_PROG_CC
AC_PROG_INSTALL
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
