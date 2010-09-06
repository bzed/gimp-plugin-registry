
ifeq (1,$(strip $(shell if dpkg --compare-versions `dpkg -s libgimp2.0-dev | awk '/^Version/ {print $$2}'` lt 2.7~; then echo 1; fi)))

PLUGIN = file-xmc
EXTRA_LDFLAGS = -lXcursor

include ../common.mk
endif
