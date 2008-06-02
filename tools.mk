#!/usr/bin/make -f

PLUGINS = $(shell find . -mindepth 2 -maxdepth 2 -name Makefile | \
                  sed 's,^\./,,g;s,/.*,,g;' | \
                  sort)

GIMPVER=$(shell dpkg -s libgimp2.0-dev | grep Version | awk '{print $$2}')

VERSION_FILES = $(shell find . -mindepth 2 -maxdepth 2 -name version)
DESC_FILES = $(shell find . -mindepth 2 -maxdepth 2 -name description)
COPYRIGHT_FILES = $(shell find . -path ./debian -prune -o -name copyright -print)

update-watch: watch
	for plugin in $(PLUGINS); do \
	    [ -f $$plugin/url ] || continue ;\
	    if grep -q registry.gimp.org $$plugin/url; then \
	        wget -q -O - `cat $$plugin/url` | grep '<span class="submitted">' | sed 's,^[^>]*>,,;s,<.*,,' > $$plugin/url.content ;\
	    else \
	        wget -q -O - `cat $$plugin/url` | md5sum | awk '{print $$1}' > $$plugin/url.content ;\
	    fi ;\
	done

watch:
	for plugin in $(PLUGINS); do \
	    if [ -r $$plugin/url.content ]; then \
	        url=`cat $$plugin/url` ;\
	        if grep -q registry.gimp.org $$plugin/url; then \
	            if ! [ "`wget -q -O - $$url | grep '<span class="submitted">' | sed 's,^[^>]*>,,;s,<.*,,'`" = "`cat $$plugin/url.content`" ]; then \
	                echo "$$plugin changed! $$url" ;\
	            fi ;\
	        else \
	            if ! [ "`wget -q -O - $$url | md5sum | awk '{print $$1}' `" = "`cat $$plugin/url.content`" ]; then \
	                echo "$$plugin changed! $$url" ;\
	            fi ;\
	        fi ;\
	    else \
	        echo "$$plugin: no url.content file!!!!" ;\
	    fi ;\
	done


.PHONY: update-watch watch
