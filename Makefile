# BMC-K4510 -- the machine, built without VICE.
#   make            -> build everything
#   make test       -> run the CPU wrapper test
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wno-unused-function -Icore
CORE_OBJS = core/xemu/cpu65.o core/mem.o

all: test/cputest

core/xemu/cpu65.o: core/xemu/cpu65.c core/xemu/cpu65.h core/xemu/emutools_basicdefs.h core/hypervisor.h
	$(CC) $(CFLAGS) -c -o $@ $<

core/mem.o: core/mem.c core/mem.h core/xemu/emutools_basicdefs.h

test/cputest: test/cputest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test: test/cputest
	./test/cputest

clean:
	rm -f core/*.o core/xemu/*.o test/cputest

.PHONY: all test clean
