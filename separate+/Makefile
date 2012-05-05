build: build-stamp

build-stamp:
	make -C src TARGETS="separate separate_import" CC="gcc -g $(CFLAGS)"
	touch build-stamp

install: build
	make -C src install PREFIX=$(DESTDIR)/usr TARGETS="separate separate_import"

clean:
	rm -f build-stamp
	make -C src clean

.PHONY: install clean build
