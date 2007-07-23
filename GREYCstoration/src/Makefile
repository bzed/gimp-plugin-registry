#--------------------------------------------------------------------------------------
#
#  Makefile  : Makefile for compiling CImg-based code on Unix
#
#  Copyright : David Tschumperle (http://www.greyc.ensicaen.fr/~dtschump/)
#
# -----------------------------------------------------------------------------------

# Define the CImg files to be compiled (name of the source file without the .cpp extension)
CIMG_FILES = greycstoration \
	     greycstoration4integration \

# Set correct variables and paths
CC       = g++
X11PATH  = /usr/X11R6
CFLAGS   = -Wall -W -ansi -pedantic -ffast-math -I./plugins
LDFLAGS  = -lm -lpthread

# Set CImg version number
CIMG_VERSION = 1.20

# Uncomment to perform strict CImg compilation
# (Do not try to preserve compatibility with older CImg versions)
CIMG_STRICT_FLAGS = -Dcimg_strict

# Uncomment to enable the use of the X11 library.
# (X11 is used by CImg to handle display windows)
CIMG_X11_FLAGS = -I$(X11PATH)/include -L$(X11PATH)/lib -lX11

# Uncomment to enable fast image display, using the XSHM library.
# (CIMG_X11_FLAGS must be defined)
CIMG_XSHM_FLAGS = -Dcimg_use_xshm -lXext

# Uncomment to enable screen mode switching, using the XRandr library.
# (CIMG_X11_FLAGS must be defined)
CIMG_XRANDR_FLAGS = -Dcimg_use_xrandr -lXrandr

# Uncomment to enable native support for PNG image files, using the PNG library.
#CIMG_PNG_FLAGS = -Dcimg_use_png -lpng -lz

# Uncomment to enable native support for JPEG image files, using the JPEG library.
#CIMG_JPEG_FLAGS = -Dcimg_use_jpeg -ljpeg

# Uncomment to enable native support for TIFF image files, using the TIFF library.
#CIMG_TIFF_FLAGS = -Dcimg_use_tiff -ltiff

# Uncomment to enable native support of most classical image file formats, using the Magick++ library.
#CIMG_MAGICK_FLAGS = -Dcimg_use_magick `Magick++-config --cppflags` `Magick++-config --cxxflags` `Magick++-config --ldflags` `Magick++-config --libs`

# Uncomment to enable faster Discrete Fourier Transform computation, using the FFTW3 library
#CIMG_FFTW3_FLAGS = -Dcimg_use_fftw3 -lfftw3

EXTRA_FLAGS = $(CIMG_X11_FLAGS) $(CIMG_STRICT_FLAGS) $(CIMG_XSHM_FLAGS) $(CIMG_XRANDR_FLAGS) $(CIMG_PNG_FLAGS) $(CIMG_JPEG_FLAGS) $(CIMG_TIFF_FLAGS) $(CIMG_MAGICK_FLAGS) $(CIMG_FFTW3_FLAGS)

# Compilation rules
.cpp:
	@echo
	@echo "** Compiling '$* ($(CIMG_VERSION))' with '`$(CC) -v 2>&1 | grep version`'"
	@echo
	$(CC) -o $* $< $(CFLAGS) $(LDFLAGS) $(ARCHFLAGS)
	strip $*
menu:
	@echo
	@echo "CImg Library $(CIMG_VERSION) : Examples"
	@echo "-----------------------------"
	@echo "  > gimp     : Compile a plug-in for GIMP (using 'gimptool-2.0')."
	@echo
	@echo "  > dlinux   : Compile for Linux/MacOSX, with debug informations."
	@echo "  > linux    : Compile for Linux/MacOSX, with standard options."
	@echo "  > olinux   : Compile for Linux/MacOSX, with optimizations."
	@echo
	@echo "  > dsolaris : Compile for Solaris, with debug informations."
	@echo "  > solaris  : Compile for Solaris, with standard options."
	@echo "  > osolaris : Compile for Solaris, with optimizations."
	@echo
	@echo "  > dminimal : Compile with minimal dependancies, with debug informations."
	@echo "  > minimal  : Compile with minimal dependancies, with standard options."
	@echo "  > ominimal : Compile with minimal dependancies, with optimizations."
	@echo
	@echo "  > clean    : Clean generated files."
	@echo
	@echo "Choose your option :"
	@read CHOICE; echo; make $$CHOICE; echo; echo "> Next time, you can bypass the menu by typing directly 'make $$CHOICE'"; echo;

all: $(CIMG_FILES) gimp

clean:
	rm -f greycstoration4gimp *.exe *~ $(CIMG_FILES)

# Gimp plug-in target
gimp: greycstoration4gimp.cpp
	@echo
	@echo "** Compiling 'greycstoration4gimp ($(CIMG_VERSION))' with '`$(CC) -v 2>&1 | grep version`'"
	@echo
	$(CC) -o greycstoration4gimp greycstoration4gimp.cpp `gimptool-2.0 --cflags` `gimptool-2.0 --libs` -lpthread -O3
	strip greycstoration4gimp
# Linux/Mac OSX targets
linux:
	make "ARCHFLAGS=$(EXTRA_FLAGS)" all
dlinux:
	make "ARCHFLAGS=-Dcimg_debug=2 -g $(EXTRA_FLAGS)" all
olinux:
	make "ARCHFLAGS=-O3 $(EXTRA_FLAGS)" all

# Sun Solaris targets
solaris:
	make "ARCHFLAGS=-R$(X11PATH)/lib -lrt -lnsl -lsocket $(EXTRA_FLAGS)" all
dsolaris:
	make "ARCHFLAGS=-Dcimg_debug=2 -g -R$(X11PATH)/lib -lrt -lnsl -lsocket $(EXTRA_FLAGS)" all
osolaris:
	make "ARCHFLAGS=-O3 -R$(X11PATH)/lib -lrt -lnsl -lsocket $(EXTRA_FLAGS)" all

# targets with minimal dependancies
minimal:
	make "ARCHFLAGS=-Dcimg_display_type=0" all
dminimal:
	make "ARCHFLAGS=-Dcimg_display_type=0 -Dcimg_debug=2 -g" all
ominimal:
	make "ARCHFLAGS=-Dcimg_display_type=0 -O3" all
