build:

install:
	install -m 644 $(SCRIPT) $(DESTDIR)$(PLUGINSCRIPTDIR)

.PHONY: build install 
