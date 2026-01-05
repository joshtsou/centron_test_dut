export PWD=$(shell pwd)
export ROOT_DIR=$(PWD)
SUBDIRS += $(ROOT_DIR)/opensource
SUBDIRS += $(ROOT_DIR)/mods
SUBDIRS += $(ROOT_DIR)/app
CLEANSUBDIRS=$(addsuffix .clean, $(SUBDIRS))
export OUTDIR=$(ROOT_DIR)/out
export MODDIR=$(ROOT_DIR)/mods
export PKG_INSTALL_DIR=$(OUTDIR)

DEBUG = n
CROSS = smax
CON=tcp

ifeq ($(DEBUG), y)
	CFLAGS += -DDEBUG
endif
ifeq ($(CROSS), smax)
	export CROSS_PATH=/opt/arm/arm-ca53-linux-gnueabihf-6.4
	export CROSS_COMPILE=$(CROSS_PATH)/usr/bin/arm-ca53-linux-gnueabihf-
	export CROSS_HOST=arm-ca53-linux-gnueabihf
	export CC=$(CROSS_COMPILE)gcc
	export CXX=$(CROSS_COMPILE)g++
	export AR=$(CROSS_COMPILE)ar
	export STRIP=$(CROSS_COMPILE)strip
	export LD=$(CROSS_COMPILE)ld
	export RANLIB=$(CROSS_COMPILE)ranlib
	export CFLAGS += -I$(ROOT_DIR)/include -I$(OUTDIR)/include -Wall -O2 -Wno-strict-aliasing -fno-strict-aliasing -march=armv8-a -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -ftree-vectorize -fno-builtin -fno-common
else ifeq ($(CROSS), centron)
	export CROSS_PATH=/opt/aarch64-ca53-linux-gnueabihf-8.4.01
	export CROSS_COMPILE=$(CROSS_PATH)/bin/aarch64-ca53-linux-gnu-
	export CROSS_HOST=aarch64-ca53-linux-gnu
	export CC=$(CROSS_COMPILE)gcc
	export CXX=$(CROSS_COMPILE)g++
	export AR=$(CROSS_COMPILE)ar
	export STRIP=$(CROSS_COMPILE)strip
	export LD=$(CROSS_COMPILE)ld
	export RANLIB=$(CROSS_COMPILE)ranlib
	export CFLAGS += -I$(ROOT_DIR)/include -I$(OUTDIR)/include -Wall -O2 -Wno-strict-aliasing -fno-strict-aliasing -march=armv8-a -mtune=cortex-a53 -ftree-vectorize -fno-builtin -fno-common
else
	export CROSS_HOST=
	export CC=gcc
	export AR=ar
	export CFLAGS += -I$(ROOT_DIR)/include -I$(OUTDIR)/include -Wall -O2 -Wno-strict-aliasing -fno-strict-aliasing
endif

ifeq ($(CON), tcp)
CFLAGS += -DCON_TCP
endif

.PHONY: $(SUBDIRS) $(CLEANSUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	make -C $@

clean: $(CLEANSUBDIRS)
	rm -rf $(OUTDIR)

$(CLEANSUBDIRS):
	@if [ -d $(basename $@) ];then cd $(basename $@); make clean; fi; 