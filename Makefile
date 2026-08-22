# BMC-K4510 -- the machine, built without VICE.
#   make            -> build everything
#   make test       -> run the CPU wrapper test
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wno-unused-function -Icore
CORE_OBJS = core/xemu/cpu65.o core/mem.o core/io.o core/vicke.o
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

ACME ?= $(HOME)/.local/bin/acme

all: rom/wozmon.bin rom/demo.bin test/capture test/cputest test/woztest test/maptest test/dmatest test/vicketest sdl/k4510

rom/wozmon.bin: rom/wozmon.a
	$(ACME) --cpu m65 -o $@ $<

rom/demo.bin: rom/demo.a
	$(ACME) --cpu m65 -o $@ $<

test/capture: test/capture.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

rom: rom/wozmon.bin rom/demo.bin

core/xemu/cpu65.o: core/xemu/cpu65.c core/xemu/cpu65.h core/xemu/emutools_basicdefs.h core/hypervisor.h
	$(CC) $(CFLAGS) -c -o $@ $<

core/mem.o: core/mem.c core/mem.h core/xemu/emutools_basicdefs.h
core/vicke.o: core/vicke.c core/vicke.h core/mem.h
core/io.o: core/io.c core/io.h core/mem.h core/vicke.h

sdl/k4510: sdl/main.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $^ $(SDL_LIBS)

test/cputest: test/cputest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test/woztest: test/woztest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test/maptest: test/maptest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test/dmatest: test/dmatest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test/vicketest: test/vicketest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test: test/cputest test/woztest test/maptest test/dmatest test/vicketest rom/wozmon.bin
	./test/cputest
	./test/woztest
	./test/maptest
	./test/dmatest
	./test/vicketest

clean:
	rm -f core/*.o core/xemu/*.o test/cputest test/woztest test/maptest test/dmatest test/vicketest test/capture rom/demo.bin sdl/k4510 rom/wozmon.bin

.PHONY: all test clean rom
