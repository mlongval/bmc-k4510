# BMC-K4510

**This is release alpha-0.3, titled 'Colophon'.** The release carries the
Raspberry Pi SD-card image and the handbook.

A fantasy 8/16-bit computer, built from scratch in August 2026 and running
**bare metal on a Raspberry Pi 3B+** (and on a Linux desktop under SDL2).
It is not an emulation of any real machine: its CPU, video chip, sound and
operating system are its own, and the parts it borrows — a 6502-family
instruction set, the SID — it borrows openly and then outgrows.

**Read the handbook first**: `doc/guide/k4510-guide.pdf`, the User's and
Programmer's Guide, 70 pages, every screenshot captured from the running
machine at build time. This README is the short version. Both are alpha
documentation of an alpha machine: things change, and the handbook's
first page says so.

## Installing

**Desktop (Linux):**

    git clone https://github.com/mlongval/bmc-k4510
    cd bmc-k4510 && ./setup.sh
    ./k4510

`setup.sh` installs the dependencies (gcc, SDL2, cc65, 64tass, nasm),
builds everything and runs the test battery. `./k4510` starts the
machine from the repo root; `./k4510 --no-startup.bat` skips
`/STARTUP.BAT` for that one run.

**Raspberry Pi 3B+:** download the SD-card zip from the releases page.
Use an **SDHC card (4-32 GB), specifically**: SDHC ships FAT32 from the
factory and just works. Old plain SD cards (2 GB and under) do **not**
boot (field-tested), and SDXC (64 GB+) ships exFAT and will not boot
until reformatted FAT32. Then either unzip onto the card's root
yourself, or:

    ./install-sd.sh bmc-k4510-pi3.zip /media/$USER/CARD     # mounted card
    ./install-sd.sh bmc-k4510-pi3.zip /dev/sdX --format     # wipe + FAT32 + install

The machine needs ~5 MB. **Use the official 5.1 V / 2.5 A supply**: a
phone charger sags under three busy cores, the firmware caps the clock,
and the machine crawls while looking like a software fault. Insert, power
on, and the K4510 boots in about three seconds.

## The machine

- **CPU: the 45GS10** — the MEGA65's 45GS02 instruction set (Q register,
  32-bit flat addressing, 28-bit MAP) plus the K4510 MMU: bank registers,
  a far-call gate, RAM under the ROM — at 40.5 MHz on the desktop, 15 MHz
  on the Pi 3B+ (a setting, F7 → Machine; `BENCH` finds the right one for
  a host). The instruction core is Xemu's, byte for byte; everything
  around it is ours.
- **Memory: 256 MB**, flat, 28-bit. The CPU sees 64 KB at a time and
  everything else is one instruction away. Byte-pokeable **bank
  registers** ($D600) and a **far-call gate** ($DF00) make programs
  bigger than the window overlays rather than a puzzle; **sideways ROM**,
  Beeb-style, pages 8 KB banks of operating system through $A000-$BFFF;
  programs own $0800-$CFFF and $E000-$FEFF by default. EhBASIC boots with
  47103 bytes free.
- **VICKY**, the video chip: 640×480, 640×240 and 320×240 (and two
  smaller fields a program may ask for), 256 colours from 24-bit, four
  layers (bitmap / tile / text), 128 sprites with no per-line limit, a
  blitter with copy/fill/logic/line/triangle ops, and **SHEILA**, a
  display-list coprocessor in the Amiga copper's tradition. No video RAM:
  every pointer is a 28-bit address.
- **Sound:** four reSID 6581s. (OPL2 is planned, not fitted.)
- **MATH unit:** eight IEEE-single registers with in-place ops and the
  transcendentals, a MEGA65-compatible multiplier/divider, and **math
  lists** — programs the unit runs by itself.
- **JIM**, the terminal: a VT100/ANSI in hardware at $DA00, drawing on the
  console's screen. CP/M programs, BBC BASIC, the editors and `TELNET`
  all write to it.
- **The network:** a URL is a file name (the Meatloaf rule) — `TYPE`,
  `LOAD`, `CP`, `RUN` and both BASICs' `LOAD` take `http://` and
  `https://`; `CD tnfs://host/dir` puts the current directory on a TNFS
  server (FujiNet, Meatloaf); the **N: device** at $D900 gives programs
  four channels (`tcp://`, `http://`), and `TELNET host port` is the
  demonstration — ANSI BBSes with their art and colours. No FTP. `https`
  is desktop-only (no TLS on the Pi); the Pi's Ethernet port is untested.

## The software

- **K/OS** (pronounced 'chaos'), the operating system, in the ROM: a
  shell with directories, `HELP` for the whole command set (the text is
  `/.HELP`), `MON`/`WOZ` the Wozmon-style monitor, `INFO`, `MODE 0-2`,
  `ALIAS`, `SWAP` (run a program on a clean machine and get this one
  back), `EXEC` scripts and `/STARTUP.BAT` at power-on (skip it from the
  F7 menu, Shell → Run STARTUP.BAT, or with `--no-startup.bat`). An
  unknown word runs `name.prg` from disk with its arguments — the REXX
  rule; `SAY` is the demo. Files live in `fs/`, one directory per
  language; bare names are searched across them.
- **Four tongues:** **EhBASIC 2.22** in ROM with graphics, sprites, the
  MATH unit and `*command` for any shell command (`*VI` edits the program
  in memory); **BBC BASIC** — Richard Russell's interpreter (BBCTTY, zlib)
  on **the Tube**, a co-processor port of Acorn heritage, with its own flat
  256 MB; **Forth** — Tali Forth 2, native 45GS10 code; and **CP/M 2.2**
  on the Tube's Z80 (RunCPM, MIT) — drives `A:`-`P:` are folders under
  `fs/CPM/`, `K:` is the machine's own filesystem, `CPM command` runs a
  program or a `.SUB` at boot, and the arrow keys arrive as the WordStar
  diamond so 1984's software can use them. The Tube runs on the desktop
  and on the Pi's core 3.
- **Two Pascals**, kept apart: Turbo Pascal 3 on CP/M (yours to supply,
  it is Borland's; drive `P:` is where it goes), and **Mad Pascal**, a
  cross-compiler — `pascal/` holds the K4510 target, `make pascal` turns
  `demo/pas/*.pas` into `fs/PRG/*.prg`; Write/CRT go through JIM, `uses
  k4510` gives every chip as a typed variable, `single` runs on the MATH
  unit, `uses graph` draws with the blitter.
- **Two editors:** `EDIT`, the nano of this machine, and `VI`, modal,
  with counts, operators, unlimited undo, `:s`, `:map`/`:imap`, a
  `/SYSTEM/VI.RC` startup file, and the whole file in far memory — 32000
  lines. `*SWAP EDIT name` edits from inside a BASIC.
- **SID player:** `SIDPLAY`, a chooser over `fs/SID`.
- **F7** opens the settings menu (C64u-style: video, audio, keys, save
  and load state, the Tube, the shell's switches; saved to `k4510.cfg`).
  Super+PageUp resets. Esc is RUN/STOP, Shift+Esc quits the emulator.
- **When something goes wrong:** `DUMP ON`, make it go wrong again, then
  `BUG` — it interviews you and writes a finished report to
  `/SYSTEM/BUGREPORTS/`, with the build, the machine and the last dump
  filled in. Attach that and the dump to an issue. Appendix B of the
  handbook is the whole of it.

## Build and run (desktop)

    make            # gcc, SDL2, cc65, 64tass, nasm
    make test       # the test battery; also checks that no tracked binary is stale
    ./k4510         # the machine, from the repo root

## Raspberry Pi 3B+

`pi/` holds the Circle kernel, the host glue and the C64 keyboard driver.
It builds against a circle-libsdl2 checkout with its `rpi3` world built
(`make BOARD=rpi3 SHIM=/path/to/circle-libsdl2`), and `pi/make-sd.sh`
lays out a card. Drive the TV at 640×480 (`pi/config.txt`).

## Documentation

- **The handbook** — `doc/guide/k4510-guide.pdf`, built and tracked in
  the repo. Source in `doc/guide/`, built with `doc/guide/make-guide.sh`:
  screenshots first, from the machine itself; Appendix A generated from
  the register comments in `core/*.h`; the GitHub issue template
  generated from Appendix B; and the build fails on a missing figure, a
  line off the page, or a `BUG` question the book does not know about.
- **The diary** — `docs/BUILD-LOG.md`, every session with its reasoning.
- **The design records** — `docs/`, mapped in `docs/README.md`.
- **Credits and terms** — `CREDITS.md` (thanks), `LICENSES.md` (the legal
  record), `THIRD_PARTY_SOURCES.md` (where each vendored component came
  from, and how to check it), `LICENSE`.
- **Filing an issue** — the handbook's Appendix B, or the template the
  issues page offers, which asks the same questions `BUG` does.

## Layout

    core/xemu/   the CPU core from Xemu (GPL-2.0-or-later), unchanged
    core/        memory, I/O devices, VICKY, SID glue, MATH unit, JIM, the network, host seam
    core/resid/  reSID (GPL-2.0-or-later)
    sdl/         the frontend (desktop and Pi alike) + POSIX host glue
    pi/          Circle kernel, Circle host glue, C64 keyboard, SD layout
    rom/         system ROM (cc65) and Wozmon
    demo/        programs in C -> fs/PRG/*.prg  (the editors, TELNET, BUG, the demos)
    basic/       EhBASIC 2.22 + K4510 glue
    forth/       Tali Forth 2 (vendored) + the platform file
    tube/        Richard Russell's BBC BASIC, console edition (vendored, altered as marked)
    cpm/         RunCPM (vendored, unmodified)
    pascal/      the Mad Pascal target
    fs/          the machine's filesystem: /PRG /EHBASIC /BBCBASIC /FORTH /CPM /SID /SYSTEM
    test/        tests, headless capture and benchmark tools
    tools/       romfree.py, which measures what is left in each ROM bank
    data/        fonts (the kernel 8x8, open-roms, unscii, BESCII)
    sidfiles/    199 SID tunes; fs/SID is a symlink to them
    doc/guide/   the handbook: source, style, generators, and the built PDF
    docs/        design records and the build diary (docs/README.md maps them)

## Licence

GPL-2.0-or-later for the project (full text in `LICENSE`); Copyright
(C) 2026 Michael Longval. Components and their terms are listed in
`LICENSES.md`, the thanks in `CREDITS.md`. EhBASIC is free for
non-commercial use only — see `basic/README-EhBASIC.txt`. "BBC BASIC"
is the name of Richard Russell's interpreter; it appears here only to
identify what is vendored, and this project claims nothing in it.

This is a hobby machine offered as a gift, and it comes with **no
warranty of any kind** — see sections 11 and 12 of `LICENSE`. It is a
toy, and it is allowed to be wrong. Keep backups.

Constructive comments can be left at the repository's issues page. All
complaints, criticisms and negativity can be addressed to `/dev/null`.
