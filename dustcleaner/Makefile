

OC_LIBS=$(shell pkg-config --libs opencv)
OC_INC=$(shell pkg-config --cflags opencv)
GIMP_LIBS=$(shell gimptool-2.0 --libs)
GIMP_INC=$(shell gimptool-2.0 --cflags)

build: build-stamp
build-stamp:
	make -C src LIBS="$(OC_LIBS) $(GIMP_LIBS)" CFLAGS="$(OC_INC) $(GIMP_INC) $(CFLAGS)" dustcleaner
	touch build-stamp

clean:
	rm -f build-stamp
	-make -C src clean

install:
	install -m 755 src/dustcleaner $(DESTDIR)$(PLUGINBINDIR)

.PHONY: clean
