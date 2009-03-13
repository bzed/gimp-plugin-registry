 #
 #  File        : Makefile
 #                ( Makefile for GNU 'make' utility )
 #
 #  Description : Makefile for compiling GREYCstoration code on Unix.
 #                This file is a part of the CImg Library project.
 #                ( http://cimg.sourceforge.net )
 #
 #  Copyright   : David Tschumperle
 #                ( http://www.greyc.ensicaen.fr/~dtschump/ )
 #
 #  License     : CeCILL v2.0
 #                ( http://www.cecill.info/licences/Licence_CeCILL_V2-en.html )
 #
 #  This software is governed by the CeCILL  license under French law and
 #  abiding by the rules of distribution of free software.  You can  use,
 #  modify and/ or redistribute the software under the terms of the CeCILL
 #  license as circulated by CEA, CNRS and INRIA at the following URL
 #  "http://www.cecill.info".
 #
 #  As a counterpart to the access to the source code and  rights to copy,
 #  modify and redistribute granted by the license, users are provided only
 #  with a limited warranty  and the software's author,  the holder of the
 #  economic rights,  and the successive licensors  have only  limited
 #  liability.
 #
 #  In this respect, the user's attention is drawn to the risks associated
 #  with loading,  using,  modifying and/or developing or reproducing the
 #  software by the user in light of its specific status of free software,
 #  that may mean  that it is complicated to manipulate,  and  that  also
 #  therefore means  that it is reserved for developers  and  experienced
 #  professionals having in-depth computer knowledge. Users are therefore
 #  encouraged to load and test the software's suitability as regards their
 #  requirements in conditions enabling the security of their systems and/or
 #  data to be ensured and,  more generally, to use and operate it in the
 #  same conditions as regards security.
 #
 #  The fact that you are presently reading this means that you have had
 #  knowledge of the CeCILL license and that you accept its terms.
 #

#-------------------------------------------------------
# Define the list of files to be compiled
# (name of the source files without the .cpp extension)
#-------------------------------------------------------
GREYCSTORATION_FILES = greycstoration greycstoration4gimp

#---------------------------------
# Set correct variables and paths
#---------------------------------
GREYCSTORATION_VERSION = 1.29
X11PATH      = /usr/X11R6
CC           = g++

ifeq ($(CC),icc)
CFLAGS       = -ansi
LDFLAGS      = -lpthread
else
CFLAGS       = -Wall -W -ansi -pedantic
LDFLAGS      = -lm -lpthread
endif

#------------------------------------------------------------
# Set compilation flags allowing to customize GREYCstoration
#------------------------------------------------------------

# Flags to enable code debugging.
GREYCSTORATION_DEBUG_FLAGS = -Dcimg_debug=3 -g

# Flags to enable code optimization by the compiler.
ifeq ($(CC),icc)
GREYCSTORATION_OPT_FLAGS = -O3 -ipo -no-prec-div
else
GREYCSTORATION_OPT_FLAGS = -O3 -ffast-math
endif

# Flags to enable the use of the X11 library.
# (X11 is used by CImg to handle display windows)
# !!! For 64bits systems : replace -L$(X11PATH)/lib by -L$(X11PATH)/lib64 !!!
GREYCSTORATION_X11_FLAGS = -I$(X11PATH)/include -L$(X11PATH)/lib -lX11

# Flags to enable fast image display, using the XSHM library.
# (GREYCSTORATION_X11_FLAGS must be defined also)
GREYCSTORATION_XSHM_FLAGS = -Dcimg_use_xshm -lXext

# Flags to enable screen mode switching, using the XRandr library.
# (GREYCSTORATION_X11_FLAGS must be defined also)
GREYCSTORATION_XRANDR_FLAGS = -Dcimg_use_xrandr -lXrandr

# Flags to enable native support for PNG image files, using the PNG library.
GREYCSTORATION_PNG_FLAGS = -Dcimg_use_png -lpng -lz

# Flags to enable native support for JPEG image files, using the JPEG library.
GREYCSTORATION_JPEG_FLAGS = -Dcimg_use_jpeg -ljpeg

# Flags to enable native support for TIFF image files, using the TIFF library.
GREYCSTORATION_TIFF_FLAGS = -Dcimg_use_tiff -ltiff

# Flags to enable native support of most classical image file formats, using the Magick++ library.
GREYCSTORATION_MAGICK_FLAGS = -Dcimg_use_magick `Magick++-config --cppflags` `Magick++-config --cxxflags` `Magick++-config --ldflags` `Magick++-config --libs`

# Flags to enable the use of LAPACK routines for matrix computation
GREYCSTORATION_LAPACK_FLAGS = -Dcimg_use_lapack -lblas -lg2c -llapack

# Flags to compile on Solaris
GREYCSTORATION_SOLARIS_FLAGS = -R$(X11PATH)/lib -lrt -lnsl -lsocket

# Flags to compile on MacOSX with Carbon-based display support
GREYCSTORATION_CARBON_FLAGS = -Dcimg_display_type=3 -framework Carbon

# Set default compilation flags.
# Uncomment lines here, if you want to customize your default configuration
GREYCSTORATION_DEFAULT_FLAGS =  $(GREYCSTORATION_OPT_FLAGS) \
	          $(GREYCSTORATION_X11_FLAGS) \
                  $(GREYCSTORATION_XSHM_FLAGS) \
                  $(GREYCSTORATION_XRANDR_FLAGS) \
                  $(GREYCSTORATION_TIFF_FLAGS) \
                  $(GREYCSTORATION_PNG_FLAGS) \
                  $(GREYCSTORATION_JPEG_FLAGS) \
#                  $(GREYCSTORATION_MAGICK_FLAGS) \
#	           $(GREYCSTORATION_LAPACK_FLAGS) \

#-------------------------
# Define Makefile entries
#-------------------------
.cpp:
	@echo
	@echo "** Compiling '$* ($(GREYCSTORATION_VERSION))' with '`$(CC) -v 2>&1 | grep -i version`'"
	@echo
	$(CC) -o $* $< $(CFLAGS) $(LDFLAGS) $(ARCHFLAGS)
ifeq ($(MACOSX_APP),1)
	mkdir -p ${*}.app/Contents/MacOS
	mv ${*} ${*}.app/Contents/MacOS
endif
ifeq ($(STRIP_EXE),1)
	strip $*
endif
menu:
	@echo
	@echo "CImg Library $(GREYCSTORATION_VERSION) : Examples"
	@echo "-----------------------------"
	@echo "  > dlinux   : Compile for Linux/BSD/MacOSX, with X11 display and debug informations."
	@echo "  > linux    : Compile for Linux/BSD/MacOSX, with X11 display and standard options."
	@echo "  > olinux   : Compile for Linux/BSD/MacOSX, with X11 display and optimizations."
	@echo "  > mlinux   : Compile for Linus/BSD/MacOSX, with minimal dependancies."
	@echo "  > Mlinux   : Compile for Linux/BSD/MacOSX, with maximal dependancies."
	@echo
	@echo "  > dsolaris : Compile for Solaris, with X11 display and debug informations."
	@echo "  > solaris  : Compile for Solaris, with X11 display and standard options."
	@echo "  > osolaris : Compile for Solaris, with X11 display and optimizations."
	@echo "  > msolaris : Compile for Solaris, with minimal dependancies."
	@echo "  > Msolaris : Compile for Solaris, with maximal dependancies."
	@echo
	@echo "  > dmacosx : Compile for MacOSX, with Carbon display and debug informations."
	@echo "  > macosx  : Compile for MacOSX, with Carbon display and standard options."
	@echo "  > omacosx : Compile for MacOSX, with Carbon display and optimizations."
	@echo
	@echo "  > clean    : Clean generated files."
	@echo
	@echo "Choose your option :"
	@read CHOICE; echo; make $$CHOICE; echo; echo "> Next time, you can bypass the menu by typing directly 'make $$CHOICE'"; echo;

all: $(GREYCSTORATION_FILES)

clean:
	rm -rf *.app *.exe *~ jpeg_buffer greycstoration4gimp $(GREYCSTORATION_FILES)

greycstoration4gimp: greycstoration4gimp.cpp
	@echo
	@echo "** Compiling 'greycstoration4gimp ($(GREYCSTORATION_VERSION))' with '`$(CC) -v 2>&1 | grep version`'"
	@echo
	$(CC) -I.. -I./plugins -o greycstoration4gimp greycstoration4gimp.cpp `gimptool-2.0 --cflags` `gimptool-2.0 --libs` -lpthread $(GREYCSTORATION_OPT_FLAGS)

# Linux/BSD/Mac OSX targets
linux:
	make "ARCHFLAGS=$(GREYCSTORATION_DEFAULT_FLAGS)" "STRIP_EXE=1" all
dlinux:
	make "ARCHFLAGS=$(GREYCSTORATION_DEBUG_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS)" all
olinux:
	make "ARCHFLAGS=$(GREYCSTORATION_OPT_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS)" "STRIP_EXE=1" all
mlinux:
	make "ARCHFLAGS=-Dcimg_display_type=0 $(GREYCSTORATION_OPT_FLAGS)" "STRIP_EXE=1" all

# Sun Solaris targets
solaris:
	make "ARCHFLAGS=$(GREYCSTORATION_SOLARIS_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS)" "STRIP_EXE=1" all
dsolaris:
	make "ARCHFLAGS=$(GREYCSTORATION_DEBUG_FLAGS) $(GREYCSTORATION_SOLARIS_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS)" all
osolaris:
	make "ARCHFLAGS=$(GREYCSTORATION_OPT_FLAGS) $(GREYCSTORATION_SOLARIS_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS)" "STRIP_EXE=1" all
msolaris:
	make "ARCHFLAGS=-Dcimg_display_type=0 $(GREYCSTORATION_OPT_FLAGS)" "STRIP_EXE=1" all

# MacOsX with Carbon display targets
macosx:
	make "ARCHFLAGS=$(GREYCSTORATION_DEFAULT_FLAGS) $(GREYCSTORATION_CARBON_FLAGS)" "MACOSX_APP=1" all
dmacosx:
	make "ARCHFLAGS=$(GREYCSTORATION_DEBUG_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS) $(GREYCSTORATION_CARBON_FLAGS)" "MACOSX_APP=1" all
omacosx:
	make "ARCHFLAGS=$(GREYCSTORATION_OPT_FLAGS) $(GREYCSTORATION_DEFAULT_FLAGS) $(GREYCSTORATION_CARBON_FLAGS)" "MACOSX_APP=1" all
