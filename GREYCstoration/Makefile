
PLUGIN = greycstoration4gimp

GT_CFLAGS = `/usr/bin/gimptool-2.0 --cflags`
GT_LDFLAGS = `/usr/bin/gimptool-2.0 --libs`


build: build-stamp

build-stamp:
	g++ -o $(PLUGIN) $(PLUGIN).cpp -I/usr/include/CImg $(CFLAGS) $(GT_CFLAGS) $(GT_LDFLAGS) -lpthread
	touch build-stamp

install:
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f $(PLUGIN) build-stamp

.PHONY: install clean build
