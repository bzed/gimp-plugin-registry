

build: build-stamp
build-stamp:
	set -e; cd build; gcc -lblas -llapack \
	                      $(LDFLAGS) $(CFLAGS) \
	                      `gimptool-2.0 --libs` `gimptool-2.0 --cflags`  \
	                      *.c -o refocus
	touch build-stamp

clean:
	rm -f build-stamp
	rm -f build/*.o build/refocus

install:
	install -m 755 build/refocus $(DESTDIR)$(PLUGINBINDIR)
	install -m 755 -d $(DESTDIR)$(DOCDIR)
	install -m 644 src/doc/doc.html $(DESTDIR)$(DOCDIR)
	install -m 644 src/doc/doc.pdf $(DESTDIR)$(DOCDIR)

.PHONY: clean
