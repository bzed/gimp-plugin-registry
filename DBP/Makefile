
build: build-stamp
build-stamp:
	make -C src CPPFLAGS=$(CFLAGS) dbp
	touch build-stamp

clean:
	rm -f build-stamp
	-make -C src clean

install:
	install -m 755 src/dbp $(DESTDIR)$(PLUGINBINDIR)
	mkdir -p $(DESTDIR)$(DOCDIR)
	install -m 644 src/dbp.html src/dbp-main.png src/dbp-fileSelector.png $(DESTDIR)$(DOCDIR)

.PHONY: clean
