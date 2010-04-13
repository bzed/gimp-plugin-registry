#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([gimp-xmc-plugin], [2.0.6], [tksmashiw@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([file-xmc.c])
AC_CONFIG_HEADER([config.h])
AC_DEFINE([PLUG_IN_DATE], ["2009/04/23"], [Release date.])
AC_ARG_ENABLE(debug,
              [  --enable-debug          trun on debugging])
if test "$enable_debug" = "yes"; then
  CFLAGS="-DXMC_DEBUG $CFLAGS"
fi


# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([XMC],
                   [xcursor gimp-2.0 >= 2.6 gimpui-2.0 >= 2.6 glib-2.0 >= 2.14 ])
AC_SUBST([XMC_CFLAGS])
AC_SUBST([XMC_LIBS])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([X11/Xcursor/Xcursor.h],,AC_MSG_ERROR([Xcursor.h not found]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.

AC_CONFIG_FILES([Makefile debian/Makefile])
AC_OUTPUT
