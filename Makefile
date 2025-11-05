export PWD=$(shell pwd)
export ROOT_DIR=$(PWD)
export CC=gcc
export AR=ar
SUBDIRS += $(ROOT_DIR)/opensource
SUBDIRS += $(ROOT_DIR)/mods
SUBDIRS += $(ROOT_DIR)/app

CLEANSUBDIRS=$(addsuffix .clean, $(SUBDIRS))
export OUTDIR=$(ROOT_DIR)/out
export MODDIR=$(ROOT_DIR)/mods
export PKG_INSTALL_DIR=$(OUTDIR)
export CROSS_HOST=
export CFLAGS += -I$(OUTDIR)/include -Wall -O2

.PHONY: $(SUBDIRS) $(CLEANSUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	make -C $@

clean: $(CLEANSUBDIRS)
	rm -rf $(OUTDIR)

$(CLEANSUBDIRS):
	@if [ -d $(basename $@) ];then cd $(basename $@); make clean; fi; 