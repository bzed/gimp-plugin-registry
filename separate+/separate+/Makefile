GIMP_VER = 2.4

#USE_LCMS2 = yes

PREFIX      = /usr
INSTALLDIR  = $(PREFIX)/lib/gimp/2.0/plug-ins
#INSTALLDIR = $(HOME)/.gimp-$(GIMP_VER)/plug-ins

ifdef USE_LCMS2
  LCMS_PKG  = lcms2
  CFLAGS   += -DUSE_LCMS2
else
  LCMS_PKG  = lcms
endif

GTK_INCLUDE  = `pkg-config gtk+-2.0 --cflags`
GTK_LIB      = `pkg-config gtk+-2.0 --libs`
GIMP_INCLUDE = `pkg-config gimp-2.0 --cflags` `pkg-config gimpui-2.0 --cflags`
GIMP_LIB     = `pkg-config gimp-2.0 --libs` `pkg-config gimpui-2.0 --libs`
LCMS_INCLUDE = `pkg-config $(LCMS_PKG) --cflags`
LCMS_LIB     = `pkg-config $(LCMS_PKG) --libs`
TIFF_LIB     = -ltiff
JPEG_LIB     = -ljpeg

GETTEXT_PACKAGE = gimp20-separate

VERSION = 0.5.8
CC      = gcc -g
DEPEND  = gccmakedep
RM      = rm -f
MAKE    = make PREFIX="$(PREFIX)"

.SUFFIXES: .c .o

# If you will use with the GIMP 2.2.x, remove "-DENABLE_COLOR_MANAGEMENT".
CM      = -DENABLE_COLOR_MANAGEMENT
NLS     = -DENABLE_NLS -DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""
CFLAGS += $(GIMP_INCLUDE) $(GTK_INCLUDE) $(LCMS_INCLUDE) $(CM) $(NLS) \
          -DUSE_ICC_BUTTON
LIBS    = $(GIMP_LIB) $(GTK_LIB) $(LCMS_LIB) $(TIFF_LIB) $(JPEG_LIB)

SOURCES = $(SEPARATE_SOURCES) $(IMPORT_SOURCES) \
          $(ICC_COLORSPACE_SOURCES) $(EXTRA_SOURCES)

OBJECTS = $(SOURCES:.c=.o)

EXTRA_SOURCES = iccbutton.c lcms_wrapper.c

SEPARATE_SOURCES = separate-core.c separate-gui.c separate-export.c util.c tiff.c psd.c jpeg.c

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
	$(CC) $(LDFLAGS) $(SEPARATE_OBJECTS) -o $@ $(LIBS)

separate_import: $(IMPORT_OBJECTS)
	$(CC) $(LDFLAGS) $(IMPORT_OBJECTS) -o $@ $(LIBS)

icc_colorspace: $(ICC_COLORSPACE_OBJECTS)
	$(CC) $(LDFLAGS) $(ICC_COLORSPACE_OBJECTS) -o $@ $(LIBS)

install-catalogs:
	cd po && $(MAKE) install

install-plugins: $(TARGETS)
	install -d "$(INSTALLDIR)"
	install -c -s $^ "$(INSTALLDIR)"

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
iccbutton.o: iccbutton.h iccclassicons.h lcms_wrapper.h
lcms_wrapper.o: lcms_wrapper.h
separate-core.o: separate.h separate-core.h lcms_wrapper.h
separate-export.o: separate.h separate-export.h tiff.h psd.h jpeg.h
separate-gui.o: separate.h separate-core.h separate-export.h iccbutton.h icon.h
import.o: separate.h
icc_colorspace.o: icc_colorspace.h iccbutton.h lcms_wrapper.h
tiff.o: separate.h util.h tiff.h
psd.o: separate.h util.h psd.h
jpeg.o: separate.h util.h jpeg.h
util.o: separate.h util.h
