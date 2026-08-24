# Licences

The BMC-K4510 as a whole is distributed under the **GNU General Public
License, version 2 or (at your option) any later version** — see
`LICENSE`. Everything written for this project (the machine, VICKe,
SHEILA, the MATH unit, the ROM, the demos, the Pi port) is
Copyright (C) 2026 Michel Longval and is released under those terms.

Components with their own origins:

| Path | What | Licence |
|---|---|---|
| `core/xemu/cpu65.c`, `cpu65.h`, `cpu65_mega65_timings.h`, `cpu65ce02_disasm_tables.c` | 65xx / 45GS02 CPU core from Xemu, Gábor Lénárt | GPL-2.0-or-later |
| `core/resid/` | reSID, Dag Lem (as shipped in VICE 3.3) | GPL-2.0-or-later |
| `data/font8.bin`, built by `data/mkfont.py` | the Linux kernel's 8x8 console font (`lib/fonts/font_8x8.c`) | GPL-2.0 |
| `basic/basic.asm` | EhBASIC 2.22, Lee Davison (ca65 form via jefftranter/6502) | **free for non-commercial use**; derivatives must carry "Derived from EhBASIC" — see `basic/README-EhBASIC.txt`. It is a separate program (`fs/ehbasic.prg`), not linked with the GPL code. |
| `cpm/src/` | RunCPM (CP/M 2.2 environment with internal CCP), Marcelo Dantas "Mockba the Borg" (vendored unmodified; see `cpm/VENDORED-FROM.txt`) | MIT |
| `forth/tali/` | Tali Forth 2, Scot W. Stevenson / Sam Colwell / Patrick Surry (vendored unmodified; see `forth/tali/VENDORED-FROM.txt`) | public domain |
| ROM and `.prg` binaries | linked against the cc65 runtime (`none.lib`) | cc65's zlib-style licence |

Not in this repository but needed for the Raspberry Pi build
(`pi/Makefile` expects them beside the checkout): [Circle](https://github.com/rsta2/circle)
(GPL-3.0) and [circle-libsdl2](https://github.com/Xalior/circle-libsdl2)
(zlib; its `sdl-app.ld` is GPL-3.0). A `kernel8.img` built from them is
therefore GPL-3.0 as a whole, which GPL-2.0-or-later code permits. The
Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`)
in a distributed card image is Broadcom's, redistributable with
Raspberry Pi hardware under its own licence.

History note: until 2026-08-23 the text font was derived from a
Commodore 64 character ROM. That file and every derivative were removed
from the repository and its history before it was made public.
