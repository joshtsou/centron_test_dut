TARGET=centron_test_dut
OBJS += statemachine.o socket.o main.o $(MODDIR)/mod_ccmd.o $(MODDIR)/mod_sscmd.o
LDFLAGS += -O2 -lev -ljansson -L$(OUTDIR)/lib
BINDIR = $(OUTDIR)/bin

.PHONY: $(BINDIR)

all: $(BINDIR) $(TARGET)

$(BINDIR):
	if [ ! -d $(BINDIR) ]; then mkdir $(BINDIR); fi;

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)
	mv $(TARGET) $(BINDIR)/$(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o