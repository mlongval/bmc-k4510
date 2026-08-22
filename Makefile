# BMC-K4510 -- the machine, built without VICE.
#   make            -> build everything
#   make test       -> run the CPU wrapper test
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wno-unused-function -Icore
CORE_OBJS = core/xemu/cpu65.o core/mem.o core/vicke.o
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

all: test/cputest sdl/k4510

core/xemu/cpu65.o: core/xemu/cpu65.c core/xemu/cpu65.h core/xemu/emutools_basicdefs.h core/hypervisor.h
	$(CC) $(CFLAGS) -c -o $@ $<

core/mem.o: core/mem.c core/mem.h core/xemu/emutools_basicdefs.h
core/vicke.o: core/vicke.c core/vicke.h core/mem.h

sdl/k4510: sdl/main.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $^ $(SDL_LIBS)

test/cputest: test/cputest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test: test/cputest
	./test/cputest

clean:
	rm -f core/*.o core/xemu/*.o test/cputest sdl/k4510

.PHONY: all test clean
