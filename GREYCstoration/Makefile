
PLUGIN = greycstoration4gimp

GT_CFLAGS = `/usr/bin/gimptool-2.0 --cflags`
GT_LDFLAGS = `/usr/bin/gimptool-2.0 --libs`


build: $(PLUGIN)

$(PLUGIN):
	cd src; g++ -o ../$(PLUGIN) $(PLUGIN).cpp -I/usr/include/CImg $(CFLAGS) $(GT_CFLAGS) $(GT_LDFLAGS) -lpthread

install:
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f $(PLUGIN) 

.PHONY: install clean build
