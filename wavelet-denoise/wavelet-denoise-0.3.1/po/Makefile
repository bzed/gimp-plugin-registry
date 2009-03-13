PLUGIN = wavelet-denoise
LOCALEDIR = /usr/share/locale
LANGUAGES = de ru it et pl

# END CONFIG ##################################################################

.PHONY: all install userinstall uninstall useruninstall clean $(LANGUAGES)
.PHONY: update-po update-pot

all: $(LANGUAGES)

PO_FILES = $(foreach lang, $(LANGUAGES), $(lang).po)

$(LANGUAGES):
	msgfmt -c -v -o $@.mo $@.po

update-po: $(PO_FILES)

$(PO_FILES): $(PLUGIN).pot
	if [[ -e $@ ]]; \
	then msgmerge -U $@ $^; \
	else msginit -l $(subst .po,,$@) -o $@ -i $^; \
fi

install: $(LANGUAGES)
	for L in $(LANGUAGES); \
	do install -v -m 0644 $$L.mo "$(LOCALEDIR)/$$L/LC_MESSAGES/gimp20-$(PLUGIN)-plug-in.mo"; \
done

uninstall: $(LANGUAGES)
	for L in $(LANGUAGES); \
	do rm -vf "$(LOCALEDIR)/$$L/LC_MESSAGES/gimp20-$(PLUGIN)-plug-in.mo"; \
done

clean:
	rm -f *.po~ *.mo $(PLUGIN).pot

userinstall:
	$(warning Will not install gettext files for user installation!)

useruninstall:
	$(warning Will not uninstall gettext files for user installation!)

$(PLUGIN).pot: ../src/*.h ../src/*.c
	xgettext -C '-k_' -kN_ -d $(PLUGIN) -o $@ $^

update-pot: $(PLUGIN).pot
