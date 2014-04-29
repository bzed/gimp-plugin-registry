# guess the name of the plugin to build if not defined
PLUGINNAME := $(shell basename $(CURDIR))
ifndef PLUGIN
PLUGIN := $(PLUGINNAME)
endif

DOCDIR := /usr/share/doc/gimp-plugin-registry/$(PLUGINNAME)

