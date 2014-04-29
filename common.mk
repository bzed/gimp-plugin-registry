include common-settings.mk

GIMPTOOL = /usr/bin/gimptool-2.0

%: %.c
	CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)" LDFLAGS="$(LFGLAGS) $(EXTRA_LDFLAGS) -lm" $(GIMPTOOL) --build $<

build: $(PLUGIN)

install: build
	mkdir -p $(DESTDIR)/$(PLUGINBINDIR)
	install -m 755 $(PLUGIN) $(DESTDIR)/$(PLUGINBINDIR)

clean:
	rm -f $(PLUGIN)

.PHONY: install clean build
