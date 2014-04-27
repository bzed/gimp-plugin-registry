
build: build-stamp

build-stamp:
	make -C src CFLAGS="$(CFLAGS) $(shell gimptool-2.0 --cflags)"
	touch build-stamp

install: build
	install -m 755 -d $(DESTDIR)$(PLUGINBINDIR)
	install -m 755 -d $(DESTDIR)/usr/share/locale
	make -C src/po install LOCALEDIR=$(DESTDIR)/usr/share/locale
	install -m 755 src/src/wavelet-decompose $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f build-stamp
	make -C src clean

.PHONY: install clean build

