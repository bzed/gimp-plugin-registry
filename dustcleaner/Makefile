

OC_LIBS=$(shell pkg-config --libs opencv)
OC_INC=$(shell pkg-config --cflags opencv)
GIMP_LIBS=$(shell gimptool-2.0 --libs)
GIMP_INC=$(shell gimptool-2.0 --cflags)

build: src/dustcleaner
	make -C src LIBS="$(OC_LIBS) $(GIMP_LIBS)" CFLAGS="$(OC_INC) $(GIMP_INC) $(CFLAGS)" dustcleaner

clean:
	make -C src clean

.PHONY: clean
