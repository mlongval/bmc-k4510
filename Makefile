# BMC-K4510 -- the machine, built without VICE.
#   make            -> build everything
#   make test       -> run the CPU wrapper test
CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wno-unused-function -Icore
CXX     ?= g++
CXXFLAGS ?= -O2 -g -Wall -Icore -fno-exceptions
RESID_OBJS = $(patsubst %.cc,%.o,$(wildcard core/resid/*.cc))
CORE_OBJS = core/xemu/cpu65.o core/mem.o core/io.o core/vicke.o core/sid.o sdl/host_posix.o $(RESID_OBJS)
LDLIBS  = -lstdc++ -lm -lutil
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

ACME ?= $(HOME)/.local/bin/acme

all: rom/wozmon.bin rom/demo.bin rom/kernal.bin fs/PRG/balls.prg fs/PRG/cube.prg fs/PRG/mandel.prg fs/PRG/keytest.prg fs/PRG/sids.prg fs/PRG/sieve.prg fs/PRG/chrout.prg fs/PRG/segdemo.prg fs/PRG/romout.prg fs/PRG/sid6.prg fs/PRG/sid12.prg fs/PRG/sidplay.prg fs/PRG/say.prg fs/EHBASIC/ehbasic.prg fs/FORTH/forth.prg cpm/runcpm test/mathtest test/capture test/headless test/fstest test/romtest test/cputest test/woztest test/maptest test/banktest test/dmatest test/vicketest test/sidtest sdl/k4510

rom/wozmon.bin: rom/wozmon.a
	$(ACME) --cpu m65 -o $@ $<

rom/demo.bin: rom/demo.a
	$(ACME) --cpu m65 -o $@ $<

# System ROM: C with cc65 (65C02 output is a subset of the 45GS02)
rom/kernal.bin: rom/kernal.c rom/crt0.s rom/k4510.cfg
	cc65 -O -t none --cpu 65c02 -o rom/kernal.s rom/kernal.c
	ca65 --cpu 65c02 -o rom/kernal.o rom/kernal.s
	ca65 --cpu 65c02 -o rom/crt0.o rom/crt0.s
	ld65 -C rom/k4510.cfg -o $@ rom/crt0.o rom/kernal.o none.lib -m rom/kernal.map

test/fstest: test/fstest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/romtest: test/romtest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/bench: test/bench.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/headless: test/headless.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/capture: test/capture.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

rom: rom/wozmon.bin rom/demo.bin rom/kernal.bin

core/xemu/cpu65.o: core/xemu/cpu65.c core/xemu/cpu65.h core/xemu/emutools_basicdefs.h core/hypervisor.h
	$(CC) $(CFLAGS) -c -o $@ $<

core/mem.o: core/mem.c core/mem.h core/host.h core/xemu/emutools_basicdefs.h
sdl/host_posix.o: sdl/host_posix.c core/host.h
core/vicke.o: core/vicke.c core/vicke.h core/mem.h
core/io.o: core/io.c core/io.h core/mem.h core/vicke.h core/sid.h
core/sid.o: core/sid.cc core/sid.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<
core/resid/%.o: core/resid/%.cc
	$(CXX) $(CXXFLAGS) -Wno-unused-parameter -c -o $@ $<

sdl/k4510: sdl/main.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $^ $(SDL_LIBS) $(LDLIBS)

test/cputest: test/cputest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/woztest: test/woztest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/banktest: test/banktest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/maptest: test/maptest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/dmatest: test/dmatest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/vicketest: test/vicketest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/mathtest: test/mathtest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test/sidtest: test/sidtest.c $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: test/cputest test/woztest test/maptest test/banktest test/dmatest test/vicketest test/sidtest test/fstest test/romtest test/mathtest rom/wozmon.bin rom/kernal.bin
	./test/cputest
	./test/woztest
	./test/maptest
	./test/banktest
	./test/dmatest
	./test/vicketest
	./test/sidtest
	./test/fstest
	./test/romtest
	./test/mathtest

clean: clean-demos
clean-demos:
	rm -f $(DEMOS) demo/*.o demo/*.s demo/*.map

	rm -f core/*.o core/xemu/*.o sdl/*.o core/resid/*.o test/sidtest test/fstest test/romtest rom/kernal.bin rom/kernal.s rom/*.o rom/kernal.map test/cputest test/woztest test/maptest test/dmatest test/vicketest test/capture rom/demo.bin sdl/k4510 rom/wozmon.bin

.PHONY: all test clean rom

# Demo programs: C with cc65, .prg files (4-byte header) loaded by the ROM
DEMOS = fs/PRG/balls.prg fs/PRG/cube.prg fs/PRG/mandel.prg fs/PRG/keytest.prg fs/PRG/sids.prg fs/PRG/sieve.prg fs/PRG/chrout.prg fs/PRG/segdemo.prg fs/PRG/romout.prg fs/PRG/sid6.prg fs/PRG/sid12.prg fs/PRG/sidplay.prg fs/PRG/say.prg
demo/prg0.o: demo/prg0.s
	ca65 --cpu 65c02 -o $@ $<
demo/romcalls.o: demo/romcalls.s
	ca65 --cpu 65c02 -o $@ $<
fs/PRG/%.prg: demo/%.c demo/k4510.h demo/far.h demo/sidorch.h demo/prg0.o demo/romcalls.o demo/prg.cfg
	cc65 -O -t none --cpu 65c02 -o demo/$*.s demo/$*.c
	ca65 --cpu 65c02 -o demo/$*.o demo/$*.s
	ld65 -C demo/prg.cfg -o $@ demo/prg0.o demo/romcalls.o demo/$*.o none.lib -m demo/$*.map
# EhBASIC 2.22 as a .prg at $7000 (basic/: Lee Davison's basic.asm + K4510 glue)
# segmented program (K-03): own header + linker config, overlays at 000
fs/PRG/segdemo.prg: demo/segdemo.c demo/segdemo-header.s demo/far.h demo/k4510.h demo/prg0.o demo/romcalls.o demo/seg.cfg
	cc65 -O -t none --cpu 65c02 -o demo/segdemo.s.tmp demo/segdemo.c && mv demo/segdemo.s.tmp demo/segdemo_c.s
	ca65 --cpu 65c02 -o demo/segdemo_c.o demo/segdemo_c.s
	ca65 --cpu 65c02 -o demo/segdemo_h.o demo/segdemo-header.s
	ld65 -C demo/seg.cfg -o $@ demo/prg0.o demo/romcalls.o demo/segdemo_c.o demo/segdemo_h.o none.lib -m demo/segdemo.map

# the SID player: a cc65 program under the ROM ($E000, block 7 banked by the K4SG loader)
fs/PRG/sidplay.prg: demo/sidplay.c demo/sidplay0.s demo/sidplay-header.s demo/sidplay.cfg demo/far.h demo/k4510.h demo/romcalls.o
	cc65 -O -t none --cpu 65c02 -o demo/sidplay_c.s demo/sidplay.c
	ca65 --cpu 65c02 -o demo/sidplay_c.o demo/sidplay_c.s
	ca65 --cpu 65c02 -o demo/sidplay0.o demo/sidplay0.s
	ca65 --cpu 65c02 -o demo/sidplay_h.o demo/sidplay-header.s
	ld65 -C demo/sidplay.cfg -o $@ demo/sidplay0.o demo/romcalls.o demo/sidplay_c.o demo/sidplay_h.o none.lib -m demo/sidplay.map

fs/EHBASIC/ehbasic.prg: basic/k4510basic.asm basic/k4510gfx.asm basic/k4510file.asm basic/k4510math.asm basic/k4510expr.asm basic/basic.asm basic/basic.cfg
	ca65 -g --cpu 65c02 --feature labels_without_colons -o basic/k4510basic.o basic/k4510basic.asm
	ld65 -C basic/basic.cfg -o $@ basic/k4510basic.o
# Tali Forth 2 (public domain, vendored unmodified in forth/tali/) as a .prg
# loaded at $4000; forth/platform.asm is the whole port (I/O + memory map)
fs/FORTH/forth.prg: forth/platform.asm forth/tali/taliforth.asm forth/tali/definitions.asm forth/tali/stringtable.asm forth/tali/forth_words.asc $(wildcard forth/tali/words/*.asm)
	64tass --nostart -q forth/platform.asm -o $@

# RunCPM (MIT, vendored unmodified in cpm/src/) -- the Z80 second processor:
# CP/M 2.2 on the Tube, internal CCP (no DRI binaries), drives in fs/CPM/
cpm/runcpm: cpm/src/main.c $(wildcard cpm/src/*.h)
	cc -Wall -O2 -Wno-unused-variable -DCCP_INTERNAL -DCPU=\"cpu1.h\" cpm/src/main.c -o $@

demos: $(DEMOS) fs/EHBASIC/ehbasic.prg fs/FORTH/forth.prg
.PHONY: demos
