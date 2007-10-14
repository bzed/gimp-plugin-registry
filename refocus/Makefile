

define PRE-CONFIGURE-HOOK
	rm -f configure
	aclocal
	autoconf
endef

define CLEAN-HOOK
	rm -rf autom4te.cache
	rm -f configure aclocal.m4
endef

include ../common-autotools.mk

