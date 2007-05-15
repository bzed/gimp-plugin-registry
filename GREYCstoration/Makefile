
PLUGIN = greycstoration4gimp

build: $(PLUGIN)
	g++ -o $(PLUGIN) $(PLUGIN).cpp \
            $(CFLAGS) \
            `/usr/bin/gimptool-2.0 --cflags` \
            `/usr/bin/gimptool-2.0 --libs` \
            -lpthread

install:
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f $(PLUGIN)

.PHONY: install clean
