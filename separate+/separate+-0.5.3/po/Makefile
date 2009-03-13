LOCALEDIR = $(PREFIX)/share/locale
#LOCALEDIR = $(PREFIX)/lib/locale
GETTEXT_PACKAGE = gimp20-separate
LINGUAS = cs fr ja ru pl ko
POFILES = $(LINGUAS:%=%.po)
CATALOGS = $(LINGUAS:%=%.gmo)

.SUFFIXES: .po .gmo

all: $(CATALOGS)

clean:
	$(RM) $(CATALOGS)

.po.gmo:
	msgfmt -o $@ $<

install: $(CATALOGS)
	linguas="$(LINGUAS)"; \
	for lang in $$linguas; do \
		dir="$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
		mkdir -p "$$dir"; \
		install -m 644 -c $$lang.gmo "$$dir/$(GETTEXT_PACKAGE).mo"; \
		echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	done
	
uninstall:
	linguas="$(LINGUAS)"; \
	for lang in $$linguas; do \
		dir="$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
		$(RM) "$$dir/$(GETTEXT_PACKAGE).mo"; \
	done
