GIMP_VER = 2.4

PREFIX = /usr
INSTALLDIR = $(PREFIX)/lib/gimp/2.0/plug-ins
#INSTALLDIR = $(HOME)/.gimp-$(GIMP_VER)/plug-ins

GTK_INCLUDE = `pkg-config gtk+-2.0 --cflags`
GTK_LIB = `pkg-config gtk+-2.0 --libs`
GIMP_INCLUDE = `pkg-config gimp-2.0 --cflags` `pkg-config gimpui-2.0 --cflags`
GIMP_LIB = `pkg-config gimp-2.0 --libs` `pkg-config gimpui-2.0 --libs`
LCMS_INCLUDE = `pkg-config lcms --cflags`
LCMS_LIB = `pkg-config lcms --libs`
TIFF_LIB = -ltiff

GETTEXT_PACKAGE = gimp20-separate

VERSION = 0.5.1
CC = gcc -g
DEPEND = gccmakedep
RM = rm -f
MAKE = make PREFIX="$(PREFIX)"

.SUFFIXES: .c .o

# If you will use with the GIMP 2.2.x, remove "-DENABLE_COLOR_MANAGEMENT".
# If you don't want to merge "normal" and "to Colour" dialog,
#                                            remove "-DSEPARATE_SEPARATE".
CM = -DENABLE_COLOR_MANAGEMENT
NLS = -DENABLE_NLS -DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""
CFLAGS = $(GIMP_INCLUDE) $(GTK_INCLUDE) $(LCMS_INCLUDE) -O3 -Wall $(CM) $(NLS) \
         -DSEPARATE_SEPARATE -DUSE_ICC_BUTTON
LDFLAGS = $(GIMP_LIB) $(GTK_LIB) $(LCMS_LIB) $(TIFF_LIB)

SOURCES = $(SEPARATE_SOURCES) $(IMPORT_SOURCES) \
          $(ICC_COLORSPACE_SOURCES) $(EXTRA_SOURCES)

OBJECTS = $(SOURCES:.c=.o)

EXTRA_SOURCES = iccbutton.c

SEPARATE_SOURCES = separate.c util.c tiff.c

SEPARATE_OBJECTS = $(SEPARATE_SOURCES:.c=.o) $(EXTRA_SOURCES:.c=.o)

IMPORT_SOURCES = import.c

IMPORT_OBJECTS = $(IMPORT_SOURCES:.c=.o)

ICC_COLORSPACE_SOURCES = icc_colorspace.c

ICC_COLORSPACE_OBJECTS = $(ICC_COLORSPACE_SOURCES:.c=.o) $(EXTRA_SOURCES:.c=.o)

TARGETS = separate separate_import icc_colorspace


.PHONY: all
all: plugins catalogs

depend:
	$(DEPEND) -- $(CFLAGS) -- $(SOURCES)

clean-plugins:
	$(RM) $(OBJECTS) $(TARGETS) core *~

clean-catalogs:
	cd po && $(MAKE) clean

clean: clean-plugins clean-catalogs

catalogs:
	cd po && $(MAKE)

plugins: $(TARGETS)

separate: $(SEPARATE_OBJECTS)
	$(CC) $(SEPARATE_OBJECTS) -o $@ $(LDFLAGS)

separate_import: $(IMPORT_OBJECTS)
	$(CC) $(IMPORT_OBJECTS) -o $@ $(LDFLAGS)

icc_colorspace: $(ICC_COLORSPACE_OBJECTS)
	$(CC) $(ICC_COLORSPACE_OBJECTS) -o $@ $(LDFLAGS)

install-catalogs:
	cd po && $(MAKE) install

install-plugins: $(TARGETS)
	install -c $^ "$(INSTALLDIR)"

install: install-plugins install-catalogs

uninstall-plugins:
	targets="$(TARGETS)"; \
	for target in $$targets; do \
		$(RM) "$(INSTALLDIR)/$$target"; \
	done

uninstall-catalogs:
	cd po && $(MAKE) uninstall

uninstall: uninstall-plugins uninstall-catalogs


# Inference rules
.c.o:
	$(CC) $(CFLAGS) -c $<


#
iccbutton.o: iccbutton.h
separate.o: separate.h iccbutton.h
import.o: separate.h
icc_colorspace.o: icc_colorspace.h iccbutton.h
tiff.o: tiff.h
util.o: util.h
