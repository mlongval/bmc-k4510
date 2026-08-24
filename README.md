# BMC-K4510

**This is release number alpha-0.2, titled 'Octopodes'.**

A fantasy 8/16-bit computer, built from scratch in August 2026 and running
**bare metal on a Raspberry Pi 3B+** (and on a Linux desktop under SDL2).

- **CPU:** 45GS10 — our 4510 descendant: the MEGA65's 45GS02 instruction set
  (Q register, 32-bit flat addressing, 28-bit MAP) plus the K4510 MMU — bank
  registers, the far-call gate, RAM under the ROM — at 40.5 MHz. The
  instruction core is Xemu's 45GS02, byte for byte; everything around it is ours.

## Installing

**Desktop (Linux):**

    git clone https://github.com/mlongval/bmc-k4510
    cd bmc-k4510 && ./setup.sh

installs the dependencies (gcc, SDL2, cc65, 64tass, nasm), builds
everything, runs the 10-suite test battery and tells you how to start
the machine (`./sdl/k4510 rom/kernal.bin fs`).

**Raspberry Pi 3B+:** download the SD-card zip from the releases page.
Use an **SDHC card (4-32 GB), specifically**: SDHC ships FAT32 from the
factory and just works. Old plain SD cards (2 GB and under) do **not**
boot (field-tested), and SDXC (64 GB+) ships exFAT and will not boot
until reformatted FAT32. Then either unzip onto the card's root
yourself, or:

    ./install-sd.sh bmc-k4510-pi3.zip /media/$USER/CARD     # mounted card
    ./install-sd.sh bmc-k4510-pi3.zip /dev/sdX --format     # wipe + FAT32 + install

The machine needs ~5 MB, so the smallest SDHC card sold is plenty.
Insert, power on, and the K4510 boots in about three seconds.

- **K/OS** (pronounced 'chaos'), the operating system: a shell (`MON`/`WOZ` is the Wozmon-style monitor, `*MON` from BASIC) plus `DIR CD MKDIR RM RMDIR TYPE
  XD LOAD SAVE RENAME CP RUN EXEC HUSH INFO MODE COLOR` (`HELP` lists them; the
  text lives in the dot-hidden `/.HELP`). An unknown word runs `name.prg` from
  disk with its arguments readable via the ARGS call ($FF95) -- the REXX rule;
  `SAY` is the demo. `/!BOOT` runs as a script at power-on (a held key skips
  it). Names starting with `.` are hidden (`DIR A` shows them). Files live in a
  host directory (`fs/`) with
  one directory per language -- `/PRG` for machine code, `/EHBASIC` and
  `/BBCBASIC` for the two BASICs, `/FORTH` for Forth, `/SID` for SID tunes -- and each language's
  demos live in its own directory (no separate demos folder); bare names are
  searched in `/PRG`, `/EHBASIC`, `/BBCBASIC` and `/FORTH` too. Boots in `MODE 1 1`: 640x240, 79x29 text with a one-cell margin.
- **EhBASIC 2.22** with graphics, the MATH unit, an expression compiler,
  `LOAD`/`SAVE` of text files, `*command` (any K/OS command at the start of a line, e.g. `*DIR`,
  `*CD EHBASIC`; `@` works too), and `LOAD` from a running program chains into the loaded
  program. `LOAD "DEMOS.BAS" : RUN` and `LOAD "BENCH.BAS" : RUN` are menus.
- **Forth:** `FORTH` at the shell runs Tali Forth 2 (public domain,
  vendored unmodified in `forth/tali/`; `forth/platform.asm` is the whole
  port) as native 45GS10 code -- no Tube, just a `.prg` at $4000 talking to
  the ROM's CHROUT/CHRIN. ANS-style Forth with the interactive 65c02
  assembler and `DISASM` kept in; the dictionary has ~12.8 KB free, `BYE`
  returns to the shell with the screen and stack intact. `C@`/`C!` reach
  every register on the machine.
- **Keys on the desktop:** Esc = RUN/STOP (stops a BASIC program, or returns to
  the shell), Ctrl-C = STOP in BASIC, F12 = reset, Shift+Esc quits the emulator.
- **SID player:** `RUN SIDPLAY` -- a chooser over `fs/SID` (199 PSID tunes
  included), played by their own 6502 code on SID 0 with the player hidden
  under the ROM; +/- picks the song, Esc returns. RSID files need a real C64
  and are shown as unsupported.
- **The Tube:** a co-processor port of Acorn heritage. `BBC` (or `BBCBASIC`)
  at the shell connects the console to Richard Russell's BBC BASIC (the
  vendored BBCTTY console edition, tube/, zlib licence) running on the host
  with its own flat 256 MB: `HIMEM=PAGE+250*1024*1024` and a 200 MB `DIM`
  just work. `*QUIT` returns to the shell. Desktop only for now; the Pi
  needs the C core compiled into the kernel (planned).
- **CP/M:** `CPM` at the shell boots CP/M 2.2 on the Tube's other occupant --
  RunCPM (MIT, vendored unmodified in `cpm/src/`), the **Z80 second
  processor** Acorn sold in 1984, except this Z80 runs at host speed. The
  internal CCP (no DRI binaries) gives DIR/TYPE/ERA/REN/SAVE/USER; drives
  A:-P: are `fs/CPM/A` .. `fs/CPM/P` with user areas as subfolders -- drop
  .COM files in and run them. `EXIT` returns to the shell. Desktop only,
  like the rest of the Tube. RunCPM's `DISK/A0.zip` is a full system disk
  (DRI's ASM/MAC/DDT/ZSID/STAT/PIP/ED, MBASIC, Z80ASM, XMODEM, the BDOS
  and CCP sources...): `unzip A0.zip -d fs/CPM/` installs it; it is not
  committed here (mixed provenance -- see `fs/CPM/.gitignore`).
- **Debugging:** `DUMP [note]` in the shell (or `*DUMP note` from BASIC) writes
  `dumps/dump-NNN.txt` on the host: CPU, banks, VICKe, SIDs, the screen, the
  shell log, the last keys, the last 4096 PCs, low memory -- paste it or point
  Claude at it.
- **Memory:** 256 MB, flat 28-bit, reached by MAP, DMA and the flat forms;
  byte-pokeable **bank registers** ($D600) and a **far-call gate** ($DF00)
  so programs bigger than the 64 KB window are overlays, not a puzzle;
  **sideways ROM**, Beeb-style: the OS pages 8 KB banks through the
  $A000-$BFFF window (the ROM file is the 24 KB base plus appended
  banks; INFO and TIME already live in bank 1), so the ROM stopped
  being full forever; **RAM under the I/O page**: a MAP of block 6
  hides $D000-$DFFF and exposes RAM -- MAP is an instruction, so the
  program that hid the I/O can always bring it back;
  `LOAD` understands segmented `K4SG` files (see `demo/segdemo.c`, `demo/far.h`).
- **VICKe**, the video chip: 640×480 / 640×240 / 320×240, 256 colours from
  24-bit, four layers (bitmap / tile / text), 128 sprites with no per-line
  limit, a blitter with copy/fill/logic/**line/triangle** ops, and
  **SHEILA**, a display-list coprocessor in the Amiga copper's tradition.
  No video RAM: every pointer is a 28-bit address.
- **Sound:** four reSID 6581s at 1 MHz. (OPL2 planned.)
- **MATH unit:** eight IEEE-single registers with in-place ops, a
  MEGA65-compatible multiplier/divider, and **math lists** — programs the
  unit runs by itself (a Mandelbrot image in under a second).
- **System ROM** (24 KB, C with cc65): colour terminal, shell with
  Wozmon's grammar over the whole 28-bit space, `INFO`, `MODE`, a host
  filesystem, `LOAD`/`RUN` of `.prg` programs with a 4-byte header.
- **EhBASIC 2.22** with `GRAPHICS`, `PLOT`, `LINE`, `TRI`, `PALETTE`,
  `GCLS`, and `LOAD`/`SAVE` of plain-text programs.
- **Pi port:** three cores — devices on 0, the emulator on 1,
  presentation on 2 — through [circle-libsdl2](https://github.com/Xalior/circle-libsdl2);
  a real C64 keyboard on the GPIO (BMC64 PCB wiring).

## Build and run (desktop)

    make            # needs gcc, SDL2, cc65, and ACME for the two asm ROMs
    make test       # ten test suites
    ./sdl/k4510     # the machine; then DIR, RUN balls.prg, RUN ehbasic.prg ...

## Raspberry Pi 3B+

`pi/` holds the Circle kernel, the host glue and the C64 keyboard driver.
It builds against a circle-libsdl2 checkout with its `rpi3` world built
(`make BOARD=rpi3 SHIM=/path/to/circle-libsdl2`), and `pi/make-sd.sh`
lays out a card. Drive the TV at 640×480 (`pi/config.txt`).

## Layout

    core/xemu/   the CPU core from Xemu (GPL-2.0-or-later), unchanged
    core/        memory, I/O devices, VICKe, SID glue, MATH unit, host seam
    core/resid/  reSID (GPL-2.0-or-later)
    sdl/         the frontend (desktop and Pi alike) + POSIX host glue
    pi/          Circle kernel, Circle host glue, C64 keyboard, SD layout
    rom/         system ROM (cc65), Wozmon and a demo (ACME)
    demo/        programs in C -> fs/*.prg  (balls, cube, mandel, sids, keytest, sieve, chrout, segdemo)
    basic/       EhBASIC 2.22 + K4510 glue -> fs/ehbasic.prg
    fs/          the machine's filesystem (programs, BASIC text files; RF1-8/AHL/SIEVE.BAS are the classic benchmarks)
    test/        tests, headless screenshot and benchmark tools
    data/        the text font (from the Linux kernel's font_8x8, GPL-2.0)

## Licence

GPL-2.0-or-later for the project; components and their terms are listed
in `LICENSES.md`. EhBASIC is free for non-commercial use only — see
`basic/README-EhBASIC.txt`.

The design documents and the build diary live in a separate folder on
the author's machine and are not part of this repository.
