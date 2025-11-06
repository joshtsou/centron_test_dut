export PWD=$(shell pwd)
export ROOT_DIR=$(PWD)
export CC=gcc
export AR=ar
export CROSS_HOST
SUBDIRS += $(ROOT_DIR)/opensource
SUBDIRS += $(ROOT_DIR)/mods
SUBDIRS += $(ROOT_DIR)/app

CLEANSUBDIRS=$(addsuffix .clean, $(SUBDIRS))
export OUTDIR=$(ROOT_DIR)/out
export MODDIR=$(ROOT_DIR)/mods
export PKG_INSTALL_DIR=$(OUTDIR)
export CFLAGS += -I$(ROOT_DIR)/include -I$(OUTDIR)/include -Wall -O2 -Wno-strict-aliasing -fno-strict-aliasing

#export CROSS_PATH=/opt/aarch64-ca53-linux-gnueabihf-8.4.01
#export CROSS_LIB=$(CROSS_PATH)/lib
#export LD_LIBRARY_PATH=$(CROSS_LIB)
#export CROSS_COMPILE=$(CROSS_PATH)/usr/bin/aarch64-ca53-linux-gnu-
#export CROSS_HOST=aarch64-ca53-linux-gnu
#export CC=$(CROSS_COMPILE)gcc
#export CXX=$(CROSS_COMPILE)g++
#export AR=$(CROSS_COMPILE)ar
#export STRIP=$(CROSS_COMPILE)strip
#export LD=$(CROSS_COMPILE)ld
#export RANLIB=$(CROSS_COMPILE)ranlib

.PHONY: $(SUBDIRS) $(CLEANSUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	make -C $@

clean: $(CLEANSUBDIRS)
	rm -rf $(OUTDIR)

$(CLEANSUBDIRS):
	@if [ -d $(basename $@) ];then cd $(basename $@); make clean; fi; 