
build: build-stamp

build-stamp:
	make -C src LDFLAGS="$(LDFLAGS) -lm" CFLAGS="$(CFLAGS) $(shell gimptool-2.0 --cflags)"
	touch build-stamp

install: build
	make -C src/po install LOCALEDIR=$(DESTDIR)/usr/share/locale 
	install -d -m 755 $(DESTDIR)$(PLUGINBINDIR)
	install -m 755 src/src/wavelet-denoise $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f build-stamp
	make -C src clean

.PHONY: install clean build

