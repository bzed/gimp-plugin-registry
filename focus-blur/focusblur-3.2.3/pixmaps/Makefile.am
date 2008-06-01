BUILT_SOURCES = pixbufs.h

png_to_csource = gdk-pixbuf-csource --struct --rle --build-list

icons = \
	focusblur-model-flat.png \
	focusblur-model-spherical.png \
	focusblur-model-gaussian.png \
	focusblur-model-ring.png \
	focusblur-model-concave.png

EXTRA_DIST = $(icons)

pixbufs.h: $(icons)
	{ echo -e '#include <gdk-pixbuf/gdk-pixdata.h>\n' ; \
	$(png_to_csource) \
	$$(for i in $(icons) ; do \
		name=$${i%%.*} ; \
		name=$${name//-/_} ; \
		echo $$name $$i ; \
	done) | \
	sed -e '/pixel_data:/a(guint8 *)' ; \
	} > $@
