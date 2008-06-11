build: build-stamp

build-stamp:
	make -C src
	touch build-stamp

install: build
	make -C src install PREFIX=$(DESTDIR)/usr

clean:
	rm -f build-stamp
	make -C src clean

.PHONY: install clean build
