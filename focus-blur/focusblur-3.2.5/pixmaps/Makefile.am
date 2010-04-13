BUILT_SOURCES = pixbufs.h

icons = \
	focusblur-model-flat.png \
	focusblur-model-spherical.png \
	focusblur-model-gaussian.png \
	focusblur-model-ring.png \
	focusblur-model-concave.png

EXTRA_DIST = $(icons)

pixbufs.h: $(icons)
	{ echo -e '#include <gdk-pixbuf/gdk-pixdata.h>\n' ; \
	$(GDK_PIXBUF_CSOURCE) --struct --rle --build-list \
	$$(for i in $(icons) ; do \
		name=$${i%%.*} ; \
		name=$${name//-/_} ; \
		echo $$name $$i ; \
	done) | \
	sed -e '/pixel_data:/a(guint8 *)' ; \
	} > $@
