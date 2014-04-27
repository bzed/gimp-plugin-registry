PLUGIN=src/normalmap

build: 
	cd src; $(MAKE) -f Makefile.linux \
                       CFLAGS="$(CFLAGS) `pkg-config --cflags gtk+-2.0 gtkglext-1.0 gimp-2.0`" \
                       LDFLAGS="-lm $(LDFLAGS)" CPPFLAGS="$(CPPFLAGS)" CC="$(CC)" LD="$(CC)"

install:
	mkdir -p $(DESTDIR)$(PLUGINBINDIR)
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	$(MAKE) -C src clean

.PHONY: install clean build
