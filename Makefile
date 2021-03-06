
CFLAGS += -O2 -Wall -std=c99 -D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
LDLIBS += -lm

PROGS=db readback upd perf rect test offset pan ovl dbrot panner scaler

all: $(PROGS)

$(PROGS): common.o font_8x8.o common.h

common.o: common.h

.PHONY: strip clean

strip: $(PROGS)
	$(STRIP) $(PROGS)

clean:
	rm -f $(PROGS) *.o
