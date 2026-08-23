# BMC-K4510

**This is release number alpha-0.1 titled 'Proof-of-concept'.**

A fantasy 8/16-bit computer, built from scratch in August 2026 and running
**bare metal on a Raspberry Pi 3B+** (and on a Linux desktop under SDL2).

- **CPU:** 45GS02 — the MEGA65's 6502 descendant (4510 + Q register +
  32-bit flat addressing + 28-bit MAP), at 40.5 MHz. The core is Xemu's,
  byte for byte; everything around it is ours.
- **Memory:** 256 MB, flat 28-bit, reached by MAP, DMA and the flat forms.
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
    make test       # nine test suites
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
    demo/        programs in C -> fs/*.prg  (balls, cube, mandel, sids, keytest, sieve, chrout)
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
