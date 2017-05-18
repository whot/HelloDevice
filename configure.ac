AC_PREREQ([2.64])
AC_INIT([HelloDevice],
        [1],
        [https://github.com/whot/HelloDevice])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])
AC_USE_SYSTEM_EXTENSIONS

# Check for programs
AC_PROG_CC_C99
AC_PROG_INSTALL

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

AC_CHECK_DECL(static_assert, [],
	      [AC_DEFINE(static_assert(...), [/* */], [noop static_assert() replacement]),
              AC_MSG_RESULT([no])],
	      [[#include <assert.h>]])

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(GLIB, [glib-2.0])
PKG_CHECK_MODULES(XI, [x11] [xi >= 1.6])

PKG_CHECK_MODULES(GLIB_236, [glib-2.0 >= 2.36], HAVE_GLIB236="yes", HAVE_GLIB236="no")
if test "x$HAVE_GLIB236" != "xyes"; then
    AC_DEFINE(NEED_G_TYPE_INIT, 1, [Need g_type_init() call])
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
