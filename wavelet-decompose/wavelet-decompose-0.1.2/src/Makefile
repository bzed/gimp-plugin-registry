CFLAGS = -O3 -Wall $(shell gimptool-2.0 --cflags)
LIBS = $(shell gimptool-2.0 --libs)
PLUGIN = wavelet-decompose
SOURCES = plugin.c wavelet.c interface.c decompose.c
HEADERS = plugin.h

# END CONFIG ##################################################################

.PHONY: all install userinstall clean uninstall useruninstall

all: $(PLUGIN)

OBJECTS = $(subst .c,.o,$(SOURCES))

$(PLUGIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $*.c
	
install: $(PLUGIN)
	@gimptool-2.0 --install-admin-bin $^

userinstall: $(PLUGIN)
	@gimptool-2.0 --install-bin $^

uninstall:
	@gimptool-2.0 --uninstall-admin-bin $(PLUGIN)

useruninstall:
	@gimptool-2.0 --uninstall-bin $(PLUGIN)

clean:
	rm -f *.o *~ $(PLUGIN)
