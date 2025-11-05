OBJS += mod_ccmd.o mod_sscmd.o
LDFLAGS += -O2 -lev -ljansson -L$(OUTDIR)/lib

.PHONY: $(BINDIR)

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o