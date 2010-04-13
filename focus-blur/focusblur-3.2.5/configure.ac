#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.54)
AC_INIT([Focus Blur], [3.2.5], [das atmark dream dot japan], [focusblur])
AC_CONFIG_SRCDIR([src/gimpplugin.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([no-define no-dist-gzip dist-bzip2])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_MAINTAINER_MODE

# Default prefix is decided with pkg-config
AC_PREFIX_DEFAULT([NONE])

#################
# Check for Win32
#################

AC_MSG_CHECKING([for native Win32])
case "$target_or_host" in
  *-*-mingw*)
    os_win32=yes
    PATHSEP=';'
    ;;
  *)
    os_win32=no
    PATHSEP=':'
    ;;
esac
AC_MSG_RESULT([$os_win32])
AC_SUBST(PATHSEP)
AM_CONDITIONAL(OS_WIN32, test "$os_win32" = "yes")
AM_CONDITIONAL(OS_UNIX, test "$os_win32" != "yes")


# Checks for libraries.

# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.

AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)
if test "x$GDK_PIXBUF_CSOURCE" = xno; then
  AC_MSG_ERROR(Could not find gdk-pixbuf-csource in your PATH)
fi

###############################
# Checks for the gimp libraries
###############################

AC_ARG_WITH(gimp-prefix,
            AS_HELP_STRING([--with-gimp-prefix=DIR],
                           [search for installed gimp-2.0.pc]),
            [
case "$withval" in
  yes) AC_MSG_ERROR([give any value for --with-gimp-prefix.]) ;;
  no) with_gimp_prefix=NONE ;;
  *)
    if test "x${withval:0:2}" = "x~/" -a "X$HOME" != "X" ; then
      with_gimp_prefix="${HOME}${withval:1}"
    else
      with_gimp_prefix="$withval"
    fi
    if ! with_gimp_prefix=$(cd "$with_gimp_prefix" && pwd) ; then
      AC_MSG_ERROR([couldn't change directory in ${withval}.])
    elif ! test -f "${with_gimp_prefix}/lib/pkgconfig/gimp-2.0.pc" ; then
      AC_MSG_ERROR([gimp-2.0.pc is not installed in ${withval}/lib/pkgconfig])
    fi ;;
esac
            ], with_gimp_prefix=NONE)
if test "X$with_gimp_prefix" != "XNONE" ; then
  export PKG_CONFIG_PATH="${with_gimp_prefix}/lib/pkgconfig${PKG_CONFIG_PATH+:$PKG_CONFIG_PATH}" LD_LIBRARY_PATH="${with_gimp_prefix}/lib${LD_LIBRARY_PATH+:$LD_LIBRARY_PATH}"
fi

AM_PATH_GIMP_2_0([2.4.0], :, AC_MSG_ERROR([Test for libgimp failed.]))
if test "x$with_gimp_prefix" != "XNONE" ; then
  gimp_libdir="$(echo -n "$GIMP_LIBS" | \
    sed -e "s;^.*-L\(${with_gimp_prefix}[[^ ]]*\).*$;\1;")"
  if test -d "$gimp_libdir" ; then
    GIMP_LIBS="-Wl,-rpath,$gimp_libdir $GIMP_LIBS"
  fi
fi

### override variables (prefix options)
pkg_config_args="$pkg_name"
if test "X$prefix" != "XNONE" ; then
  pkg_config_args="$pkg_config_args --define-variable=prefix=$prefix"
elif test "X$with_gimp_prefix" != "XNONE"; then
  pkg_config_args="$pkg_config_args --define-variable=prefix=$with_gimp_prefix"
  ac_default_prefix="$with_gimp_prefix"
else
  ac_default_prefix="$($PKG_CONFIG $pkg_config_args --variable=prefix)"
fi
if test "X$exec_prefix" != "XNONE"; then
  pkg_config_args="$pkg_config_args --define-variable=exec_prefix=$exec_prefix"
fi
GIMP_PLUGIN_DIR="$($PKG_CONFIG $pkg_config_args --variable=gimplibdir)"

### user plug-in directory
GIMP_USER_PLUGIN_DIR="${HOME}/.gimp-${gimp_pkg_major_version}.${gimp_pkg_minor_version}/plug-ins"
AC_SUBST(GIMP_USER_PLUGIN_DIR)


#############################
# Threads and multi processor
#############################

AC_ARG_ENABLE(mp,
              AS_HELP_STRING([--disable-mp],
                             [disable support for multiple processors]))
if test "x$enable_mp" != "xno"; then
  PKG_CHECK_MODULES(GTHREAD, [gthread-2.0],
    AC_DEFINE(ENABLE_MP, 1, [Define to 1 to enable support for multi-thread.]))
fi


##################
# Checks for FFTW3
##################

AC_CHECK_HEADERS(complex.h)

have_fftw3=no
AC_ARG_WITH(fftw3,
            AS_HELP_STRING([--without-fftw3], [build without FFTW3 support]))
if test x$with_fftw3 != xno; then
  have_fftw3=yes
  if test -z "$FFTW3_CFLAGS" && test -z "$FFTW3_LIBS"; then
    PKG_CHECK_MODULES(FFTW3, fftw3f,
      AC_DEFINE(HAVE_FFTW3, 1, [Define to 1 if FFTW3 is available]),
                have_fftw3="no (FFTW3 not found)")
  fi
fi

AM_CONDITIONAL(HAVE_FFTW3, test "x$have_fftw3" = "xyes")

AC_SUBST(FFTW3_CFLAGS)
AC_SUBST(FFTW3_LIBS)


############
# I18N stuff
############

GETTEXT_PACKAGE="gimp20-focusblur"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The gettext translation domain.])
ALL_LINGUAS="it ja ru"
#AC_PROG_INTLTOOL
#AM_GLIB_GNU_GETTEXT
AM_GNU_GETTEXT

AC_CONFIG_FILES([Makefile src/Makefile pixmaps/Makefile po/Makefile.in])

AC_OUTPUT
