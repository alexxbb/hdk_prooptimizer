PNAME = sop_prooptimizer
OS_NAME := $(shell uname -s)
SOURCES = $(PNAME).cpp
INSTDIR = $(HOME)/houdini13.0

ifeq ($(OS_NAME),Darwin)
	DSONAME = $(PNAME).dylib
	OPTIMIZER = -O1
else
	DSONAME = $(PNAME).so
	OPTIMIZER = -O2
endif
# OPTIMIZER = -g

include $(HFS)/toolkit/makefiles/Makefile.gnu

all:	install clean
install:	default	clean
	@if [ ! -d $(INSTDIR)/dso ]; then mkdir $(INSTDIR)/dso; fi
	@mv $(DSONAME) $(INSTDIR)/dso
clean:
	@rm *.o
