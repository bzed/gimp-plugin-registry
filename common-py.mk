ifndef SCRIPT_SRCDIR
 SCRIPT_SRCDIR := $(CURDIR)
endif

build: $(SCRIPT:%=$(SCRIPT_SRCDIR)/%c)

$(SCRIPT_SRCDIR)/%c: $(SCRIPT_SRCDIR)/%
	pycompile $*


install: $(SCRIPT:%=$(DESTDIR)$(PLUGINSCRIPTDIR)/%)

$(DESTDIR)$(PLUGINSCRIPTDIR)/%: $(SCRIPT_SRCDIR)/%
	if ! test -d $(DESTDIR)$(PLUGINSCRIPTDIR); then \
		install -d -m 755 $(DESTDIR)$(PLUGINSCRIPTDIR) ;\
	fi
	install -m 755 $(SCRIPT_SRCDIR)/$* $@

clean:
	rm -f $(SCRIPT_SRCDIR)/*.pyc

.PHONY: build install 
