# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.4-p5 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999, 2001 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/sh

srcdir = /home/peter/refocus-0.9.0/src
top_srcdir = /home/peter/refocus-0.9.0
VPATH = /home/peter/refocus-0.9.0/src
prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/refocus
pkglibdir = $(libdir)/refocus
pkgincludedir = $(includedir)/refocus

top_builddir = ..

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
CC = gcc
GCC3 = 
GIMPTOOL = /usr/bin/gimptool
GIMP_CFLAGS = -I/usr/include/gimp-2.0 -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2 -I/usr/include/libpng12
GIMP_CFLAGS_NOUI = -I/usr/include/gimp-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
GIMP_DATA_DIR = /usr/share/gimp/2.0
GIMP_LIBS = -L/usr/lib -lgimpui-2.0 -lgimpwidgets-2.0 -lgimp-2.0 -lgimpcolor-2.0 -lgimpmath-2.0 -lgimpbase-2.0 -L/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0
GIMP_LIBS_NOUI = -L/usr/lib -lgimp-2.0 -lgimpcolor-2.0 -lgimpmath-2.0 -lgimpbase-2.0 -L/lib -lglib-2.0
GIMP_PLUGIN_DIR = /usr/lib/gimp/2.0
GLIB_CFLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include  
GLIB_GENMARSHAL = glib-genmarshal
GLIB_LIBS = -L/lib -lglib-2.0  
GLIB_MKENUMS = glib-mkenums
GOBJECT_QUERY = gobject-query
GTKDOC = true
GTK_CFLAGS = -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2 -I/usr/include/libpng12  
GTK_LIBS = -L/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0  
HTML_DIR = ${datadir}/gtk-doc/html
LAPACK_INCLUDE_DIR = ${top_srcdir}/lib
LAPACK_LIB_DIR = ${top_srcdir}/lib
MAKEINFO = makeinfo
PACKAGE = refocus
PKG_CONFIG = /usr/bin/pkg-config
RANLIB = ranlib
VERSION = 0.9.0

INCLUDES = -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2 -I/usr/include/libpng12   -I/usr/include/gimp-2.0 -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2 -I/usr/include/libpng12 -I ${top_srcdir}/lib

bin_PROGRAMS = refocus
noinst_PROGRAMS = test-matrix

ldadd_atlas = -L${top_srcdir}/lib -llapack -lcblas -latlas

noinst_HEADERS = bdclosure.h refocus.h matrix.h tilebuf.h conv.h fwlapack.h gimppreview.h prevman.h util.h
EXTRA_DIST = dummy-plugin.c

refocus_SOURCES = refocus.c conv.c gimppreview.c prevman.c util.c matrix.c tilebuf.c bdclosure.c fwlapack.c
refocus_LDADD = -L/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0   -lgimp ${ldadd_atlas}
refocus_DEPENDENCIES = ${top_srcdir}/lib/liblapack.a

test_matrix_SOURCES = matrix.c test-matrix.c fwlapack.c
test_matrix_LDADD = -lm ${ldadd_atlas} -L/lib -lglib-2.0  
test_matrix_DEPENDENCIES = ${top_srcdir}/lib/liblapack.a
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_CLEAN_FILES = 
PROGRAMS =  $(bin_PROGRAMS) $(noinst_PROGRAMS)


DEFS =  -DPACKAGE=\"refocus\" -DVERSION=\"0.9.0\" -DSTDC_HEADERS=1 -DRF_DEBUG=1 -DPREVIEW_DEBUG=1 -DHAVE_ERF=1  -I. -I$(srcdir) 
CPPFLAGS = 
LDFLAGS = 
LIBS = 
refocus_OBJECTS =  refocus.o conv.o gimppreview.o prevman.o util.o \
matrix.o tilebuf.o bdclosure.o fwlapack.o
refocus_LDFLAGS = 
test_matrix_OBJECTS =  matrix.o test-matrix.o fwlapack.o
test_matrix_LDFLAGS = 
CFLAGS = -Wall -ansi -pedantic -ggdb -fomit-frame-pointer -O3 -funroll-all-loops
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
HEADERS =  $(noinst_HEADERS)

DIST_COMMON =  Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = gtar
GZIP_ENV = --best
SOURCES = $(refocus_SOURCES) $(test_matrix_SOURCES)
OBJECTS = $(refocus_OBJECTS) $(test_matrix_OBJECTS)

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .o .s
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu --include-deps src/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-binPROGRAMS:

clean-binPROGRAMS:
	-test -z "$(bin_PROGRAMS)" || rm -f $(bin_PROGRAMS)

distclean-binPROGRAMS:

maintainer-clean-binPROGRAMS:

install-binPROGRAMS: $(bin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "  $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	     $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	  else :; fi; \
	done

uninstall-binPROGRAMS:
	@$(NORMAL_UNINSTALL)
	list='$(bin_PROGRAMS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	done

mostlyclean-noinstPROGRAMS:

clean-noinstPROGRAMS:
	-test -z "$(noinst_PROGRAMS)" || rm -f $(noinst_PROGRAMS)

distclean-noinstPROGRAMS:

maintainer-clean-noinstPROGRAMS:

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

refocus: $(refocus_OBJECTS) $(refocus_DEPENDENCIES)
	@rm -f refocus
	$(LINK) $(refocus_LDFLAGS) $(refocus_OBJECTS) $(refocus_LDADD) $(LIBS)

test-matrix: $(test_matrix_OBJECTS) $(test_matrix_DEPENDENCIES)
	@rm -f test-matrix
	$(LINK) $(test_matrix_LDFLAGS) $(test_matrix_OBJECTS) $(test_matrix_LDADD) $(LIBS)

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = src

distdir: $(DISTFILES)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
bdclosure.o: bdclosure.c bdclosure.h util.h
conv.o: conv.c refocus.h util.h tilebuf.h prevman.h conv.h matrix.h \
	bdclosure.h
dummy-plugin.o: dummy-plugin.c
fwlapack.o: fwlapack.c fwlapack.h util.h ../lib/f2c.h ../lib/clapack.h
gimppreview.o: gimppreview.c gimppreview.h
matrix.o: matrix.c fwlapack.h matrix.h refocus.h util.h
prevman.o: prevman.c prevman.h util.h
refocus.o: refocus.c gimppreview.h refocus.h prevman.h matrix.h conv.h \
	tilebuf.h bdclosure.h util.h
test-matrix.o: test-matrix.c matrix.h refocus.h util.h
tilebuf.o: tilebuf.c tilebuf.h prevman.h util.h
util.o: util.c util.h

info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am: install-binPROGRAMS
install-exec: install-exec-am

install-data-am:
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-binPROGRAMS
uninstall: uninstall-am
all-am: Makefile $(PROGRAMS) $(HEADERS)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(bindir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-binPROGRAMS mostlyclean-noinstPROGRAMS \
		mostlyclean-compile mostlyclean-tags \
		mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-binPROGRAMS clean-noinstPROGRAMS clean-compile \
		clean-tags clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-binPROGRAMS distclean-noinstPROGRAMS \
		distclean-compile distclean-tags distclean-generic \
		clean-am

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-binPROGRAMS \
		maintainer-clean-noinstPROGRAMS \
		maintainer-clean-compile maintainer-clean-tags \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-binPROGRAMS distclean-binPROGRAMS clean-binPROGRAMS \
maintainer-clean-binPROGRAMS uninstall-binPROGRAMS install-binPROGRAMS \
mostlyclean-noinstPROGRAMS distclean-noinstPROGRAMS \
clean-noinstPROGRAMS maintainer-clean-noinstPROGRAMS \
mostlyclean-compile distclean-compile clean-compile \
maintainer-clean-compile tags mostlyclean-tags distclean-tags \
clean-tags maintainer-clean-tags distdir info-am info dvi-am dvi check \
check-am installcheck-am installcheck install-exec-am install-exec \
install-data-am install-data install-am install uninstall-am uninstall \
all-redirect all-am all installdirs mostlyclean-generic \
distclean-generic clean-generic maintainer-clean-generic clean \
mostlyclean distclean maintainer-clean


install-bin: refocus
	@if test "x/usr/bin/gimptool" = "x" ; then \
		echo "Warning: configure did not find gimptool" ; \
		echo "install src/refocus manually in your plug-in directory" ; \
		echo "e.g.: cp src/refocus ~/.gimp-1.2/plug-ins" ; \
	else \
		/usr/bin/gimptool --install-bin refocus ; \
	fi

install-admin-bin: refocus
	@if test "x/usr/bin/gimptool" = "x" ; then \
		echo "Warning: configure did not find gimptool" ; \
		echo "install src/refocus manually in your plug-in directory" ; \
		echo "e.g.: install src/refocus /usr/local/lib/gimp-1.2/plug-ins" ; \
	else \
		/usr/bin/gimptool --install-admin-bin refocus ; \
	fi

${top_srcdir}/lib/liblapack.a:
	cd ${top_srcdir}/lib; make

%.s: %.c
	${CC} ${CFLAGS} -S $< -o $@

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
