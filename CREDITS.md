# Credits and acknowledgments

LICENSES.md is the legal record; this file is the thanks.

The BMC-K4510 is a fantasy machine, but it stands on very real work.
Thank you:

## Code running in the machine

- **Gábor Lénárt** (LGB) — the 45GS02 CPU core from
  [Xemu](https://github.com/lgblgblgb/xemu), used unchanged. The
  heart of the machine.
- **Dag Lem** — [reSID](https://en.wikipedia.org/wiki/ReSID), the
  SID emulation behind all four sound chips (as shipped in VICE 3.3).
- **Lee Davison** (1966–2013) — EhBASIC, the machine's first tongue.
  Derived from EhBASIC.
- **R.T. Russell** — BBC BASIC (console edition), running on the Tube
  co-processor. The name "BBC BASIC" is used by his permission.
- **Marcelo Dantas** ("Mockba the Borg") —
  [RunCPM](https://github.com/MockbaTheBorg/RunCPM), the Z80 second
  processor's CP/M.
- **Scot W. Stevenson, Sam Colwell, Patrick Surry** —
  [Tali Forth 2](https://github.com/SamCoVT/TaliForth2).
- **Steve Wozniak** — Wozmon, still the best 256 bytes ever written.
- **The cc65 project** — the compiler, assembler, and runtime the
  system ROM is built with.
- **Tomasz Biela** ("tebe") — [Mad Pascal](https://github.com/tebe6502/Mad-Pascal)
  and [MADS](https://github.com/tebe6502/Mad-Assembler) (both MIT), the
  cross-Pascal the K4510 target in `pascal/` plugs into; and
  **Wojciech Bociański** ("bocianu"), whose Neo6502 target showed the way.

## Fonts

- **The Linux kernel** — the 8x8 console font that got the text mode
  on its feet.
- **Ville-Matias Heikkilä** ("Viznut") —
  [unscii](http://viznut.fi/unscii/), placed in the public domain.
- **Paul Gardner-Stephen and Roman Standzikowski** (FeralChild64) —
  the clean-room chargen from
  [MEGA65 open-roms](https://github.com/MEGA65/open-roms).
- **Retrofan** — PXLfont (included via open-roms, with permission).
- **Damian Vila** — [BESCII](https://codeberg.org/Dmian/font-bescii),
  the PETSCII spirit with a clean pedigree, CC0.

## The Raspberry Pi port

- **Randy Rossi** — [BMC64](https://github.com/randyrossi/bmc64), VICE
  on a bare-metal Raspberry Pi with 50 Hz smooth scrolling and
  single-frame input latency. The reason this machine has a Pi port at
  all: the route there was always meant to be BMC64's `emux_api` seam.
  Also the **VIC-II Kawari** — a modern drop-in VIC-II with modes the
  original never had, and the proof that extending an 8-bit machine's
  video chip is working inside the tradition, not outside it. VICKY is
  a more reckless cousin of that idea.
- **minch** (aminch) — BMC64's maintainer since Randy handed it over,
  carrying it onto the newer Pis.
- **Rene Stange** — [Circle](https://github.com/rsta2/circle), the
  bare-metal Pi environment.
- **Xalior** —
  [circle-libsdl2](https://github.com/Xalior/circle-libsdl2).

## The workshop

The machine is built with **cc65**, **64tass**, **NASM**, **GCC** and
**GNU Make**, and shows itself to you through **SDL2**. The guide is
set in **XeLaTeX** with **Clear Sans** (Intel) and **Iosevka**
(Belleve Invis), and every screenshot in it is captured from the
machine actually running — a picture that cannot be produced fails the
build.

## Heritage and inspiration

- **The MEGA65 project** — for keeping the 45GS02 and the C65 dream
  alive, and for open-roms.
- **Acorn Computers** — the Tube, the sideways ROM model, and the
  `*` prefix. The Beeb's ghost is all over this machine.
- **Commodore** — the other half of the machine's soul: PETSCII,
  SIDs, and the 8-bit line from PET to C65.
- **Michael Steil** — [msbasic](https://github.com/mist64/msbasic),
  and **Microsoft** for the 2025 MIT release of 6502 BASIC v1.1
  (planned: the machine's next native BASIC).
- **The VICE team** — decades of emulation scholarship this project
  leans on constantly.
- **Kim Lemon** and the Lemoners — [Lemon64](https://www.lemon64.com)
  has been the C64's front door since 1998: games database, reviews and
  scans, music, and a forum that answers. Twenty-eight years of
  dedication to a machine's community, and a good half of the small
  facts this project needed.
- **The Neo6502 handbook** and the **MEGA65 User's Guide** — the two
  books the guide is modelled on, down to the page size.
- **The High Voltage SID Collection** and the composers in it, whose
  tunes are what `SIDPLAY` plays on a good evening. None of that music
  is distributed with this machine (see LICENSES.md); it is theirs.

## The machine's other author

Most of the K4510's own code — VICKY, SHEILA, the DMA engine, the ROM,
K/OS, the Tube ULA, the editors, the Pi port — was written in
conversation with Claude (Anthropic's Claude Code), over several months
of long sessions with a build log to prove it. The design decisions are
the author's; a great deal of the typing was not.

## And whoever is missing

This list was assembled by hand and is certainly incomplete. If your
work is in this machine and your name is not here, that is an error and
not a judgement — open an issue and it will be fixed.
