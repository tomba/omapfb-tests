
ifdef CROSS_COMPILE
	CC=$(CROSS_COMPILE)gcc
endif

CFLAGS=-O2 -Wall
LDFLAGS=-lm

PROGS=db readback upd perf rect test offset pan ovl dbrot panner

all: $(PROGS)
	$(CROSS_COMPILE)strip $(PROGS)

.c.o: common.h font.h

test: test.o common.o font_8x8.o
upd: upd.o common.o font_8x8.o
ovl: ovl.o common.o font_8x8.o

clean:
	rm -f $(PROGS) *.o
