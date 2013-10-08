
ifdef CROSS_COMPILE
	CC=$(CROSS_COMPILE)gcc
	LD=$(CROSS_COMPILE)ld
endif

CFLAGS += -O2 -Wall -std=c99 -D_BSD_SOURCE -D_XOPEN_SOURCE
LDLIBS += -lm

PROGS=db readback upd perf rect test offset pan ovl dbrot panner

all: $(PROGS)
	$(CROSS_COMPILE)strip $(PROGS)

$(PROGS): common.o font_8x8.o common.h

common.o: common.h

clean:
	rm -f $(PROGS) *.o
