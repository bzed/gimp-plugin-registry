## Process this file with automake to produce Makefile.in

bin_PROGRAMS = dustcleaner

bindir = $(GIMP_LIBDIR)/plug-ins

dustcleaner_SOURCES = \
	plugin-intl.h	\
	dust.h	\
	dustcleaner_gimp_plugin.c	\
	dustcleaner_gimp_plugin.h	\
	dustanalyzer.c	\
	interface.c	\
	interface.h	\
	render.c	\
	render.h	\
	dustfinder.c	\
	recovery.c	\
	utilities.c	\
	main.c		\
	main.h		

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"		\
	-DDATADIR=\""$(DATADIR)"\"

INCLUDES =\
	-I$(top_srcdir)		\
	@GIMP_CFLAGS@		\
	-I$(includedir)		\
	`pkg-config --cflags opencv` 

LDADD = $(GIMP_LIBS) 

