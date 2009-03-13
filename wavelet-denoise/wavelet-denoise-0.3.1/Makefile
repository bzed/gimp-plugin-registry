.PHONY: all po src install userinstall clean dist install uninstall

all: po src

po:
	$(MAKE) -C po
src:
	$(MAKE) -C src

install:
	$(MAKE) -C po install
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C po uninstall
	$(MAKE) -C src uninstall

userinstall:
	$(MAKE) -C po userinstall
	$(MAKE) -C src userinstall

useruninstall:
	$(MAKE) -C po useruninstall
	$(MAKE) -C src useruninstall

clean:
	$(MAKE) -C po clean
	$(MAKE) -C src clean

dist:
	$(error make dist not implemented!)
