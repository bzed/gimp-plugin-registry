
build: build-stamp

build-stamp:
	make -C src 
	touch build-stamp

install: build
	make -C src/po install LOCALEDIR=$(DESTDIR)/usr/share/locale CFLAGS=$(CFLAGS)
	install -m 755 src/src/wavelet-denoise $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f build-stamp
	make -C src clean

.PHONY: install clean build

