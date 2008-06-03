build:

install:
	install -m 755 $(SCRIPT) $(DESTDIR)$(PLUGINBINDIR)

.PHONY: build install 
