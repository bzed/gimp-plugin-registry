
SCRIPT = $(shell if dpkg --compare-versions $(GIMPVER) '>=' 2.3; then \
                    echo planet-render1-2.scm ;\
                  else \
                    echo planet-render1-1.scm ;\
                 fi)

include ../common-script.mk
