SUBDIRS_y+=libev-4.33
SUBDIRS_y+=jansson-2.7
#SUBDIRS_y+=jansson-2.13
DEBUGSUBDIRS = $(addsuffix .debug, $(SUBDIRS_y))
RELEASESUBDIRS = $(addsuffix .release, $(SUBDIRS_y))
CLEANSUBDIRS = $(addsuffix .clean, $(SUBDIRS_y))
DISTCLEANSUBDIRS = $(addsuffix .distclean, $(SUBDIRS_y))
INSTALLSUBDIRS = $(addsuffix .install, $(SUBDIRS_y))

.PHONY: $(SUBDIRS_y) $(DEBUGSUBDIRS) $(RELEASESUBDIRS) $(INSTALLSUBDIRS) \
	$(CLEANSUBDIRS) $(DISTCLEANSUBDIRS)

all: $(SUBDIRS_y)

clean:	$(CLEANSUBDIRS)
	@rm -rf *.o *.so *.a

distclean: $(DISTCLEANSUBDIRS)

$(DISTCLEANSUBDIRS):
	@cd $(basename $@) && make distclean
	
$(SUBDIRS_y):
	@echo
	echo ==========================================
	echo opensource: $@
	echo ==========================================
	@echo Making all in subdirectory $@...
	cd $@ && if [ ! -f Makefile ]; then ./cfg.sh; fi && make all install
	echo ==========================================
	echo opensource after install: $@
	echo ==========================================

install: $(INSTALLSUBDIRS)

$(DEBUGSUBDIRS):
	@echo
	@echo Executing make debug in subdirectory $(basename $@)...
	@cd $(basename $@) && make debug

debug:	$(DEBUGSUBDIRS)

$(RELEASESUBDIRS):
	@echo
	@echo Executing make release in subdirectory $(basename $@)...
	@cd $(basename $@) && make release

release:	$(RELEASESUBDIRS)

$(INSTALLSUBDIRS):
	@echo
	@echo Executing make install in subdirectory $(basename $@)...
	@cd $(basename $@) && make install

depend:

$(CLEANSUBDIRS):
	@cd $(basename $@) && make clean



