PLUGIN=src/normalmap

build: $(PLUGIN)
	$(MAKE) -C src CFLAGS="$(CFLAGS) `pkg-config --cflags gtk+-2.0 gtkglext-1.0 gimp-2.0`" \
                       LDFLAGS="$(LDFLAGS)" CPPFLAGS="$(CPPFLAGS)" CC="$(CC)" LD="$(CC)"

install: build
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	$(MAKE) -C src clean

.PHONY: install clean build
