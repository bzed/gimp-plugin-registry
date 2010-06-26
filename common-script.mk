build:

install: $(SCRIPT:%=$(DESTDIR)$(PLUGINSCRIPTDIR)/%)

$(DESTDIR)$(PLUGINSCRIPTDIR)/%:
	if ! test -d $(DESTDIR)$(PLUGINSCRIPTDIR); then \
		install -d -m 755 $(DESTDIR)$(PLUGINSCRIPTDIR) ;\
	fi
	install -m 644 $* $@

.PHONY: build install 
