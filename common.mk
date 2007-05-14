GIMPTOOL = /usr/bin/gimptool-2.0
GIMPTOOL_CFLAGS = $(shell $(GIMPTOOL) --cflags)
GIMPTOOL_LDFLAGS = $(shell $(GIMPTOOL) --libs)

%: %.c:
	gcc $(GIMPTOOL_CFLAGS) $(CFLAGS) -o $@ $< $(GIMPTOOL_LDFLAGS) $(LDFLAGS)
