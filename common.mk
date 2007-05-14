GIMPTOOL = /usr/bin/gimptool-2.0

%: %.c
	CFLAGS=$(CFLAGS) $(GIMPTOOL) --build $<

build: $(PLUGIN)

install:
	install -m 755 $(PLUGIN) $(DESTDIR)$(PLUGINBINDIR)

clean:
	rm -f $(PLUGIN)

.PHONY: install clean
