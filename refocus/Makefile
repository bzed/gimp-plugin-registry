

define PRE-CONFIGURE-HOOK
	set -e; cd src; rm -f configure; aclocal; autoconf
endef

define CLEAN-HOOK
	set -e; cd src; rm -rf autom4te.cache; rm -f configure aclocal.m4
endef

define POST-INSTALL-HOOK
	install -d -m 755 $(DESTDIR)$(DOCDIR)
	install -m 644 src/doc/doc.html $(DESTDIR)$(DOCDIR)
	install -m 644 src/doc/doc.pdf $(DESTDIR)$(DOCDIR)
endef

EXTRA_CFLAGS := $(shell /usr/bin/gimptool-2.0 --cflags)
EXTRA_LDFLAGS := $(shell /usr/bin/gimptool-2.0 --libs)

include ../common-autotools.mk

