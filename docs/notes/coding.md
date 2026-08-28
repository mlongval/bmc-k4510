# Coding session — status

Protocol: `docs/AGENTS.md`. I write only this file.

**Updated: 2026-08-26 14:5x**

## Now

- Nothing in flight.

## Waiting on

- Nothing from the handbook session.
- From Doc: whether `CPM K-TURBO` still gives `A0>` interactively. Both
  of us have now failed to reproduce it from a script, so if it persists
  it is something about the live session, not the launcher.

## Parked for the Pi 4 / core-3 work — cpmemu (2026-08-27, added at Doc's request)

Doc found <https://github.com/rsta2/cpmemu> — René Stange's own CP/M 2.2
emulator. rsta2 wrote Circle, so this is the same two-target shape we
have: `kernel.cpp` + `multicore.cpp` for bare-metal Pi via Circle, and a
`Makefile.linux` for a plain Linux build from the same sources. MIT
throughout, clean per-file headers.

**Not a RunCPM replacement.** It runs the real DRI CCP/BDOS
(`system/ccp.hex`, `system/bdos.hex`, assembled from the PL/M source with
MAC) over rsta2's own `bios.asm`, on Lin Ke-Fong's `z80emu` core. Real
BDOS means real sectors, so drives are CP/M disk images built with its
`cpmdisk` tool. Our K:/P:/D: symlink drives, `CPM [command]` via
AUTOEXEC.TXT and the `K-*.SUB` launchers all exist *because* RunCPM traps
BDOS and points it at host directories — none of that survives a swap.
Keep RunCPM.

Worth reading anyway, for three things:

- **`multicore.cpp`** — running the Z80 on its own Circle core, by the
  person who wrote Circle. Same technique as SID on core 3.
- A second, independent worked example of emulator-on-Circle bring-up.
- **Real-BIOS fidelity.** If a CP/M program ever misbehaves under RunCPM
  because it calls the BIOS jump table directly instead of going through
  BDOS, this is the reference for what should happen.

Its "no terminal control sequences, characters pass through unchanged"
caveat does not apply to us — JIM at $DA00 already does VT100/ANSI.

If any of it is ever vendored: the MIT header covers rsta2's code only.
`ccp.hex` and `bdos.hex` are DRI binaries under the Caldera/Lineo grant
and would need their own line in `THIRD_PARTY_SOURCES.md`.

## Every machine current, on binaries as well as the tree (2026-08-27)

Doc, today: "I move around a lot from machine to machine, thus all
machines must be in sync on repo and binaries."

He sits down at whichever machine is nearest and runs what is on it. A
`git pull` leaves a host looking current while its `sdl/k4510`,
`rom/kernal.bin` and `fs/PRG/*.prg` are from whenever it last built.
Three separate faults today came from exactly that: he tested a reported
regression against an emulator thirty commits old; p15's `rom/kernal.bin`
was a day behind and would have gone onto an SD card; and thirteen tracked
`.prg` files were stale on every host at once because `make` skipped them.

So, for both of us:

- **Pulled is not built.** After a change lands, rebuild every host that
  can build, not just the one you are sitting at. A host left
  pulled-but-unbuilt is worse than one left behind, because it lies.
- **`rom/kernal.bin` is untracked and needs cc65.** It does not arrive with
  a pull. p15 has no cc65, so it must be sent the binary; `pi/make-sd.sh`
  now refuses a card whose ROM is older than `rom/kernal.c`.
- **`rm -f demo/*.o` before trusting a rebuild**, and before believing
  `make check-artifacts`. An intermediate that looks newer than its source
  is skipped, and the guard then compares a stale artifact against itself.
- **Tracked artifacts get built once and confirmed twice**: build on one
  host, verify byte-identical on a second, then commit.
- **Say which hosts are current** when you report, and which are not. "It
  is pushed" is not the same statement as "it runs there".

Toolchains count as machine state too: cc65 on ubuntu-s1, t480i5 and
hdieu; the aarch64 15.2 toolchain on p15; ACME being installed on
ubuntu-s1 and hdieu today. A host that cannot build a thing should be
written down as such rather than discovered at the moment it matters.

## REQUIREMENT: every host carries every tool (2026-08-27)

Doc: "all required tooling must be available on all hosts." A host that
cannot build part of the tree is a host that will surprise someone, and
since he sits down at whichever machine is nearest, the surprise is his.
`make all` and `make test` must both complete on all four.

Two ways a tool goes missing, and the second is the one that keeps
catching us:

1. **It is not installed.** hdieu had no Mad Pascal, so `make all` died on
   `fs/PRG/hello.prg` there; p15 had no cc65, nasm or SDL2 until today.
2. **It is installed somewhere a non-interactive shell cannot see.** cc65
   lives in `~/opt/cc65/bin` on ubuntu-s1 and hdieu, ACME and nasm in
   `~/.local/bin`, and none of those are on the PATH an `ssh host command`
   gets. Every build on those hosts today needed the PATH set by hand.
   **A tool off the PATH is a missing tool** as far as any script, cron
   job or agent is concerned.

The state as of 2026-08-27, after installing what was absent:

| tool | what needs it | ubuntu-s1 | t480i5 | hdieu | p15 |
|---|---|---|---|---|---|
| cc65 / ca65 / ld65 | ROM, all `.prg` | `~/opt/cc65/bin` | `/usr/bin` | `~/opt/cc65/bin` | `/usr/bin` |
| SDL2 dev | the desktop emulator | yes | yes | yes | yes |
| nasm | `tube/bbcbasic` | `~/.local/bin` 2.16.03 | `/usr/bin` 2.16.03 | `/usr/bin` 3.02 | `/usr/bin` 3.02 |
| ACME 0.97 | `rom/wozmon.bin`, `rom/demo.bin` | `~/.local/bin` | `~/.local/bin` | `~/.local/bin` | `~/.local/bin` |
| 64tass 1.60.3243 | `fs/FORTH/forth.prg` | `~/.local/bin` | `/usr/local/bin` | `/usr/bin` | `/usr/bin` |
| Mad Pascal 1.7.8 | `demo/pas/*.pas` | `~/Projects/neo6502_dev/` | `~/Projects/neo6502_dev/` | `~/Projects/neo6502_dev/` | `~/Projects/neo6502_dev/` |
| Mad Assembler 2.1.8 | the same | `~/Projects/neo6502_dev/` | `~/Projects/neo6502_dev/` | symlink to `~/src/` | `~/Projects/neo6502_dev/` |
| Free Pascal | rebuilding `mp` | `~/opt/fpc` 3.2.2 | `/usr/bin` 3.2.2 | `/usr/bin` 3.2.3 | `/usr/bin` 3.2.3 |
| aarch64 15.2 | the Pi kernel | - | - | - | `~/opt/arm-gnu-...` |
| XeLaTeX | the handbook | `/usr/bin` | **MISSING** | **MISSING** | **MISSING** |

Closed 2026-08-27. Ubuntu has no 64tass 1.60 and ubuntu-s1 has no
passwordless sudo, so it is built from the SourceForge source into
`~/.local/bin`; Fedora's package is the same version. XeLaTeX is the one
open line: a large install for a document only ubuntu-s1 has ever built,
and worth asking Doc about rather than assuming either way.

**Mad Pascal is no longer a precious binary (2026-08-27, second pass).**
Free Pascal is now on all four, so `pascal/install.py` does the whole job
and the compiler can be rebuilt anywhere. The recipe, in full:

    git clone https://github.com/tebe6502/Mad-Pascal.git DIR
    git -C DIR checkout 4a0e5bcd          # the pin: upstream 1.7.8 "optimizations"
    python3 pascal/install.py DIR         # patches the source, copies pascal/mp/*, builds mp

Nothing in it is unbackupable: the K4510 half (`lib/k4510.pas`,
`graph_k4510.inc`, `base/k4510/`, `rtl6502_k4510.asm` and the rest) is
copied out of `pascal/mp/` in THIS repository, and the eleven upstream
files it edits are edited by that script. Only the upstream commit needed
pinning, and 4a0e5bcd is it.

**Tested, because the fpc versions differ**: p15 built `mp` with Fedora's
fpc 3.2.3 and ubuntu-s1 with the official 3.2.2 tarball, and both produce
`hello.prg` and `pgraph.prg` BYTE-IDENTICAL to what is committed. So the
fpc version does not reach the generated 6502 code, and needs no pin of
its own.

One wrinkle: `demo/pas/*.lst` records mads's `-i:` path in its second
line, so the listings only reproduce when Mad Pascal sits at the canonical
`~/Projects/neo6502_dev/Mad-Pascal`. Build it elsewhere and you get a
one-line diff that is not a code difference.

ubuntu-s1 has no passwordless sudo, so its fpc is the official
`fpc-3.2.2.x86_64-linux.tar` installed into `~/opt/fpc`, with a symlink at
`~/.local/bin/fpc` so a non-interactive shell finds it.

**Historical, and still true of a stock clone:** I tried, and it is a
trap: `make pascal-install` runs `pascal/install.py`, which PATCHES the
Mad Pascal source (`src/include/syntax.inc` gains `-target:k4510`,
`src/mp.pas` moves the stack, `lib/system.pas` puts the transcendentals
on the MATH unit) and then rebuilds `mp` with Free Pascal — which is on
no host. On a stock clone the rebuild fails and leaves no working
compiler at all. Worse, a stock `bin/linux_x86_64/mp` is 1.7.5-Test and
generates DIFFERENT CODE from the patched 1.7.8: `demo/pas/pgraph.lst`
came out 1023 lines different on hdieu before I noticed. **Copy the whole
patched tree** (`rsync -a --exclude=.git`, 72 MB) from a host that has
one; the patches live in the tree, not only in the binary. All four now
reproduce `hello.prg`, `pgraph.prg`, the listings and `forth.prg` byte
for byte.

**Version pins that matter**, learned the hard way: ACME must be 0.97 or
later (0.96.4 does not know `--cpu m65`, which `rom/wozmon.a` needs, and
the GitHub mirror is stuck there); nasm 2.16.03 reproduces `tube/bbcbasic`
byte-identically where 3.02 produces a different, working binary.

## p15: the Pi kernel and the desktop build fight over the same objects

`pi/Makefile` and the desktop Makefile both write `core/*.o`,
`core/xemu/*.o`, `core/ui/*.o`, `core/resid/*.o` and `demo/*.o` — one set
aarch64, the other x86-64. Whichever ran last owns them, and the other
link fails or, worse, does not. **On p15, clean those before switching
between `make -C pi` and a desktop `make`.** It is the only host where
both are built.

## For the handbook agent — user-visible, shipped today, undocumented

**The "hold a key to skip STARTUP.BAT" message is gone**, and so is the
half-second grace window it announced (Doc: "F7 will suffice"). Boot goes
straight in. The two ways to skip it are now F7 -> Shell -> Run
STARTUP.BAT, which persists, and `--no-startup.bat`, which does not. If
the book mentions holding a key at the banner, that is now wrong.

**INFO reports the architecture, and pages.** It now reads:

    SYSTEM  K/OS stage 4 (the BMC-K4510 operating system)
            build 0.3-044bac5+, desktop emulator

or `bare metal on a Raspberry Pi 3B+`. It used to say "emulator"
unconditionally, which was simply wrong on the Pi. It is 28 lines and did
not fit MODE 3's 25 rows, so it pages like TYPE now.

**INFO stays in the ROM** (sideways bank 1) rather than becoming a .prg,
if the book ever has cause to say where things live: a .prg has to load
from the filesystem, and INFO is the thing you want when the filesystem
is what is broken. Same reason DUMP is in ROM.

**`BUG` is `fs/PRG/bug.prg`, not a shell command** (Doc's design: an
interview, not a printed template). `BUG` or `*BUG` from anywhere — the
REXX rule finds it. It asks seven questions, fills in five lines itself,
and writes a finished report.

- File: **`/SYSTEM/BUGREPORTS/BUG-YYYYMMDD-HHMMSS.TXT`** from the machine,
  `fs/SYSTEM/BUGREPORTS/` from the host (which is the path a user needs to
  upload it). The directory is made if absent.
- Machine-answered: `Machine` (desktop / Raspberry Pi 3B+, from `$D522`),
  `Version`, `Screen` (size and MODE), `Dump` (the last DUMP, or a nudge to
  go and type DUMP first), `When`.
- Asked: the labels are `bug_labels[]` in **`demo/bug.c`**, marked with a
  `BUG-LABELS:` comment for your check. That is the file and the symbol.
- Zero ROM: it is a program.

**`Version` now identifies the exact build.** `sys_version` is stamped by
the Makefile from `git rev-parse --short`, with `+` for a dirty tree — so
`INFO` and every BUG report now read e.g. `0.3-044bac5+` instead of
`k4510 0.3`. That is the change you flagged as the one that would make
reports traceable; it cost nothing and only `core/io.o` rebuilds.

**`$D522`** is in `core/io.h` as `IO_SYS_HOST`, commented in the shape the
rest of that block uses.

**`TYPE` (and so `HELP`) pages** — a screenful at a time, `-- more --`,
any key continues, `q` or Esc stops. **Not** when a script is running it,
or STARTUP.BAT would hang waiting for a key nobody is there to press.

**`fs/PRG/logo.prg`** — the banner as a program (`RUN PRG/LOGO`), beside
the ROM's `LOGO` command. It writes text32 cells directly, so it can set
backgrounds, and it reads the geometry from JIM so it follows the MODE
and the margin. The bars, colours and text are four tables at the top of
`demo/logo.c`, meant to be edited.

**MODE 4 no longer hangs the machine.** `pad()` could be asked for a
column past the right margin, which `cx` can never reach.

**`k4510 --no-startup.bat`** (also `--no-startup`, or `K4510_NO_STARTUP=1`
in the environment) skips `/STARTUP.BAT` for that run only. The F7 switch
does the same but persists, and holding a key at the banner needs you to
be there; this is the one-shot for a script, or for a startup file that
wedges the machine and you want one clean boot to go and fix it. It does
not touch `k4510.cfg`.

**Video → Scaling** in the F7 menu: the three choices are now what their
names say. `sharp` is nearest with free scaling (sharp, but at a window
size that is not a whole multiple it drops pixel rows unevenly);
`soft` is linear; `sharp-fit` is nearest **and** integer scaling, so every
pixel of the machine is a whole number of pixels on the glass — exact,
with letterboxing instead of artefacts. sharp-fit was picking linear as
well, which threw away the point of it.

**`LOGO` (new command)** clears the screen and reprints the startup
banner. It lives in the sideways window (SWCODE0), not the resident ROM,
so it cost 24 bytes of ROM2 for its dispatch and nothing else.

**The banner itself changed.** Five colour bars stepped 4:3:2:3:4, and
new text beside them. (Tapered ends were tried and removed: at this cell
size a one-pixel diagonal beside a solid bar reads as dirt.)

    BMC-K4510 -- A FANTASY 8/16-bit COMPUTER
    CPU: 45GS10 at 40.5 MHz + runCPM Tube
    RAM: 256 000 000 bytes
    CHIPS: 4 reSID, VICKY, SHEILA, FRED, JIM

If the book has a picture of the old banner it is now wrong.

Everything here is in the machine and is not in the book yet.

**VI (`demo/vi.c`, 270e8c6)** grew most of an editor. The header comment
in that file is the authoritative key list. In brief:

    counts   3dd 5j 2dw 10G      before almost anything
    move     w b e ^  (added to h j k l 0 $ G gg PgUp PgDn)
    operate  d c y + any motion, or doubled: dd cc yy
    insert   s S C  (added to i a I A o O)
    change   x X r ~ J D  p P (unnamed register, filled by d/x/y)
    undo     u    redo Ctrl-R      (unlimited; the journal is far memory)
    search   /pat ?pat n N         plain substrings, not regex
    ex       :s/old/new/[g]   :%s/old/new/[g]
             :map lhs rhs    :imap lhs rhs      (:imap jk <Esc>)

Two things worth a sentence in the book: charwise operators **clamp to
the line** (`dw` at a line end does not eat into the next), and search is
substrings, not patterns — both deliberate.

**EhBASIC `*VI` and `*EDIT` (3d2967b)** with nothing after them edit the
program in memory: SAVE to a temp file, run the editor under SWAP, LOAD
it back. With an argument they are the ordinary `*` escape. Variables do
not survive (it is a LOAD). Costs 512 bytes of BASIC RAM: 47103 free
becomes 46591. I put a section in `doc/guide/chapters/03-basic.tex` —
yours to keep, rewrite or move.

**F7 menu (ed0904d, and earlier)** gained Video → Resolution and
Video → Left/top margin, and Shell → Run STARTUP.BAT. Resolution shows
what the machine is actually in and follows a `MODE` you type. The menu
offers only the first three modes: MODE 3 and 4 are reachable by command
but not from the menu, and never saved.

**`MODE 0-4`** — 640x480, 640x240, 320x240, 320x200 (40x25, the C64's
geometry), 160x200. Raster lines always count the full 480, so in MODE 3
the first line of the picture is raster line 40.

**STARTUP.BAT** has no grace window and no message any more (Doc: "F7
will suffice"). The F7 menu's *Run STARTUP.BAT* toggle and
`--no-startup.bat` are the two ways out of one that wedges the machine.

**Caps lock** behaves like a caps lock: shifted gives the other case. It
is also suspended while a program runs, so `:q` reaches VI.

## Recently finished

- `prctl(PR_SET_PDEATHSIG)` so the Tube's children die with the emulator
  (6998246, include now guarded by `__linux__` as you suggested).
- `THIRD_PARTY_SOURCES.md` (7706dce) — provenance per vendored
  component, and a list of the six whose upstream version is unpinned.
- Deleted `test/shot`, the stray tracked x86 binary. Thank you.

## alpha-0.3 "Colophon" — released

Released at 05fafb7, as a pre-release, with two assets: the Pi SD-card
zip and the handbook PDF. I rebuilt `doc/guide/k4510-guide.pdf` with
`GUIDE_VERSION=alpha-0.3 ./make-guide.sh --no-shots` so the cover carries
the release rather than `alpha-0.2+117`, and committed just that file —
no figures recaptured, and `mkregs.py`/`mkissue.py` produced no drift.
That is the only thing I have touched under `doc/`. The PDF is also a
release asset, alongside the Pi SD-card zip.

Three things there that touch your side:

- The PDF asset is the book as of that commit, cover stamped `alpha-0.3`
  rather than `alpha-0.2+117`. If you rebuild it, `GUIDE_VERSION` is how
  the cover gets a release name before the tag exists.
- `fs/.HELP` was stale in the machine, not just in the book: it offered
  `MODE 0-2`, still told you to hold a key to skip STARTUP.BAT, and had
  never heard of `BUG` or `LOGO`. Fixed. If the book quotes HELP
  anywhere, it is worth a look.
- The card's CP/M drives ship empty on purpose (mixed provenance), and
  the Pi's network side has never run on real hardware. Both are said
  plainly in the release notes and in `pi/README-SD.txt`; if the book
  claims either works on the Pi, it is ahead of the evidence.

---

## Status-bar text mode (2026-08-27)

Built the first of the three "terminal" features Doc scoped: a real
hardware status-bar mode, F7-selectable. 640x240 / 80x30, but the middle
25 rows are a scroll region between two static bands (Doc's 2 top + 3
bottom). It is a DECSTBM done in the machine's own layout: `scroll()`
already only moved `OY..OY+ROWS-1`, so once the geometry carves a top band
(OY) and a bottom band (`bband`), the console scrolls in the window while
the bands sit still. Proven headless: two `DIR PRG` listings scroll the
first up and off the top of the window and the bands never move.

Wiring, for whoever extends it:
- **Orthogonal BOOL, like `margin`** — `SET_VIDEO_STATUSBAR` (settings.c/h,
  a Video menu row), host packs `SYSOPT_STATUS` ($D521 bit 3) every frame
  (main.c), and suppresses `SYSOPT_MARGIN` while it is on (bands frame the
  screen, no margin with them). `status_shown` reconciles like `margin_shown`
  so toggling it issues a mode request; the boot branch requests it too.
- **ROM (kernal.c):** `OY`/`bband` are now variables (OY was `#define
  margin`). `video_init` reads `$D521 & SYSOPT_STATUS` live and, for the
  80-column modes only, sets `OY = PROWS/15`, `bband = PROWS/10` (→ 2+3 at
  640x240, 4+6 at 640x480 = the future 80x50). `bband != 0` is the "status
  is on" flag the rest of the ROM reads. `cls` clears only the window and
  calls `draw_bands`; `mode_do` forces `margin = 0` under status.
- **`draw_bands` must be RESIDENT** (ROM1C) — `cls` runs inside bank-1
  context (cmd_mode calls it) and `sw_call` does not nest, so it cannot be
  banked. It cost ~570 bytes of ROM1C; watch that headroom. Trap I hit:
  `modename()` lives in SWCODE1, so a resident caller gets garbage — the
  resolution string had to be inlined, then dropped for budget. Phase-1
  content is "BMC-K4510 K/OS" (top) and "status mode ... NN MHz" (bottom,
  the live clock). The blank spacer rows are the phase-2 widget slots.
- Geometry falls out for free: the console reports `ROWS = 25`, so NC / EDIT
  / VI confined to it just work (they read `REG(TERM+6)`); they cannot draw
  into the bands. That is the [[project-k4510-terminal-mode-and-filer]]
  geometry rule, satisfied by construction.
- Not done: live band refresh (the clock is drawn at mode entry, not per
  frame — change the F7 clock and the band is stale until the next `cls`);
  configurable widgets; the 80x50 tall variant is reachable (640x480 +
  status) but untested. `MODE` from the shell still can't toggle status
  (F7 only); INFO's 28 lines page inside the 25-row window.

---

## ZX Origins fonts in the menu + the native clock tick (2026-08-28)

**Clock now ticks from the IRQ.** The status-bar clock rode the console key
poll, so it froze inside a running program (a BASIC compute loop never asks
for a key). Moved it to the vblank IRQ that already blinks the cursor
(crt0.s clk_paint): every ~half-second it checks the RTC minute and repaints
the eight digit cells of HH:MM DD.MM straight into the text map, borrowing
$02-$05 like the blink, never touching the ZP-swap core. Proven ticking
inside `10 GOTO 10` across a midnight rollover. Gotcha that cost time:
adding code before `_speed_loop` shifted it so its inner branch spanned a
page (+1 cycle/iter x 37000 = INFO -c reading 38.3 not 40.4 MHz) -- moved
_speed_loop ahead of the IRQ so later edits can't move it. Measurement is
placement-sensitive; keep it early.

**Twelve ZX Origins fonts (F7 -> Screen font).** FONT_ZX_* in settings.h,
font_names[] in settings.c, paths[] in apply_font (sdl/main.c) -- three
lists in lockstep, plus tools/mkzxfonts.py. Each ZX Origins zip ships a
4096-byte C64/<name>.bin; the machine's petscii_to_ascii reads the
lower/upper charset from offset 2048, but ZX Origins stores it first, so the
tool SWAPS the two halves. Without the swap the text renders as line-drawing
characters (set 1 = "both"/lower-upper at offset 0, set 2 = "upper"/graphics
at 2048 -- opposite of a standard C64 chargen).

**Licence -- the files are NOT committed.** ZX Origins (Damien Guard, (c)
1988-2023) is free to use but forbids re-hosting the files. The repo is
public, so data/fonts/zx/*.bin is gitignored, like Commodore's chargen.bin
("names them, does not ship them"). Regenerate with tools/mkzxfonts.py from
your own download; absent -> the menu entry falls back to kernel8. make-sd.sh
now copies data/fonts/ so the Pi gets alternate fonts too (also fixes
unscii/openroms never reaching the card before).

## For the handbook agent -- please credit Damien Guard

Doc asked (2026-08-28) that the handbook thank the ZX Origins author. When
the font menu is documented, add a credit like:

  "Screen fonts from ZX Origins by Damien Guard (https://damieng.com/zx-origins),
   used with thanks."

The dozen offered are Bauhaus, Broadway, Computer, Cyberwire, NLQ, Benguiat,
Chicago, Courier, Eurostile, OCR-A, Pristine, Anvil. The licence's own
suggested form is "<fontname> font by DamienG https://damieng.com/zx-origins".
Note the fonts are NOT shipped with the machine (licence forbids re-hosting) --
the book should say the machine offers them and where to get them, not imply
they are bundled.

## Roadmap note, 2026-08-28

Doc raised nine ideas at once (Turbo Pascal/VI, a TNFS server, hosting
resources, a font tool, the codepage, HELP pages, USB, a mouse cursor, a
GUI). Thought through in `docs/notes/roadmap-2026-08-28.md` — nothing built
yet, no code touched. Audited against the tree the same day after Doc
challenged one claim; seven corrections are marked in place there.

**For the handbook agent — §6, HELP pages.** Doc would like `HELP DIR` to
explain `DIR`. Proposal: pages as `/SYSTEM/HELP/<CMD>.TXT`, plain text, 80
columns, NAME/SYNOPSIS/DESCRIPTION/EXAMPLES/SEE ALSO; the handbook's command
reference and those files generated from one source, plus a build guard that
compares the pages against the `is_cmd(&p, "…")` dispatch table in
`rom/kernal.c` and fails on either kind of gap. Coding side owes the
`HELP <topic>` dispatch and the directory. Read §6 before starting; say in
your note if you would rather shape it differently.

**Found by the audit, worth acting on — `petscii_to_ascii` blanks 136 glyph
slots.** `sdl/main.c:79` fills 0x20-0x7F plus a hand table of 24 box glyphs
and zeroes the rest, so selecting *any* 4096-byte chargen (open-roms, PXLfont,
ZX Origins, a real C64 chargen.bin) loses 134 of the glyphs `data/font8.bin`
carries — 27 of them in CP437's 0xB0-0xDF line/block range — and draws the
double-line box with single-line glyphs. KOMMANDER happens to use only six
glyphs, all mapped, which is why it has not shown. Fix planned: extend the
table and fall back to the kernel font for unmapped slots. **Handbook: any
figure shot with a non-default font may show blanks — worth knowing before
recapturing.**

**Decision that gates the font work — §5.** Recommendation: CP437 stays the
machine's native codepage; PETSCII becomes a VICKY charset + screen-code
translation *mode*, not a second encoding. Consequence for the handbook: the
F7 font menu will only offer fonts normalised to full CP437 coverage, so a
ZX Origins font there is ZX letters with borrowed box-drawing — worth a
sentence when that menu is documented.

## KOMMANDER — a two-panel file commander, 2026-08-28

Built `demo/kommander.c` → `fs/PRG/kommander.prg`, typed `KOMMANDER`. The
Norton Commander of this machine: two directory panels, Tab switches the live
one, arrows/PgUp/PgDn/Home/End walk it, Enter descends a directory or views a
file, `..` climbs. Function keys **F3** View, **F5** Copy, **F6** Move/rename,
**F9** MkDir, **F10** Delete; `.` toggles dotfiles, **F2** refreshes, **Esc**
leaves. (F7 and F8 never reach a program — they are the menu and pause keys —
so MkDir/Delete moved off Norton's F7/F8 to F9/F10.)

Two design facts drove it, both worth remembering:

- **A `.prg` cannot launch another `.prg`.** Both load at $6000, so shelling
  out to EDIT would overwrite the running program and crash on its return
  (`run_at` invokes a program with a plain `jsr $6000` and expects `rts`). So
  KOMMANDER is wholly self-contained: every file operation goes straight to the
  storage device at $D300 (DIR_FIRST/NEXT, STAT, LOAD, CHDIR, MKDIR, RM, RMDIR,
  RENAME, COPYFILE, GETCWD), and it has its own read-only pager for View.
- **The device has one current directory**, but a commander needs two. Kept an
  invariant: the device cwd always tracks the *active* panel. To list the idle
  panel it `CHDIR`s there (absolute paths work — `fs_resolve` honours a leading
  `/`), lists, and `CHDIR`s back. Each panel remembers its own absolute path
  from GETCWD.

Drawn straight into VICKY text32 at $030000 (glyph-lo, glyph-hi, fg, bg per
cell) inside the ROM console window — geometry from JIM's registers
(COLS/ROWS/OX/OY/PCOLS at $DA05..$DA0D), so it lives correctly under the status
bands and in the margin. Single-line CP437 frames (the default font8 carries the
whole box set). Blue field, cyan bar on the active panel, grey on the idle one.
Exits with the EDIT idiom — `REG($DA04)=2; rom_video()` — to hand the shell a
clean screen.

Entry tables are BSS: 192 entries/panel, 30-char names (bigger and the image
overruns the $6000–$CFFF program region). Long names truncate; deep dirs cap at
192 with the rest unseen. The `.prg` is ~11 KB.

**For the handbook agent:** unbuilt when you last looked — this is now a real,
shipping program worth a short section (a file manager alongside EDIT/VI). It is
keyboard-only, self-contained, and does not launch other programs (the $6000
reason above). Recapture any figure that shows the program list — `KOMMANDER` is
in `/PRG` now.

**Roadmap §5b added (2026-08-28) — multicolour fonts.** Text/text32 are
1 bpp hard-coded in `core/vicky.c:122`; only tile mode does 2/4/8 bpp.
VICKY-SPEC §5 promises bpp-aware text; unbuilt. Proposal is bpp-aware
text32 (pixel 0 = bg, 1 = fg, 2..N = palette). **Handbook:** the spec and
Appendix A should not be read as describing shipped behaviour here —
text mode is 1 bpp until this lands.

## TINY — a scrolling tile map with sprites, 2026-08-28

Doc: "a nice scrolling tile map demo with sprites -- check the internet for
example and assets you can download freely". Built `demo/tiny.c` →
`fs/PRG/tiny.prg`, typed `TINY`. Kenney's **Tiny Dungeon** (CC0) vendored in
`data/tinydungeon/`; `tools/mktiny.py` turns the 132-tile sheet into VICKY
16x16 8 bpp tiles and the Tiled sample map into a 64x40 tile map (the sample
tiled 2x2, flip bits carried over); both ride in as a K4SG segment at
$00110000, so the program is 4.6 KB of code and 38 KB of art. Layer 0 tile
mode in 320x240, a text8 caption on layer 1, 41 sprites (a knight you steer
plus 40 wanderers) — 8 bpp sprites and 8 bpp tiles share one layout, so the
sprite DATA pointers aim straight into the tile set. Camera follows the
knight; arrows turn him, space stops, Esc/Q leave. Walls are found by colour
(`walkable[]` in the generated tiny.h). 60 fps on the desktop; not yet run on
the Pi. kenney.nl itself refuses curl (403); the OpenGameArt mirror served
the zip.

**I edited three shared files — CREDITS.md, LICENSES.md,
THIRD_PARTY_SOURCES.md — one row/record each for Tiny Dungeon.**

**For the handbook agent:** a new program in `/PRG`, worth a figure — it is
the machine's first tile-mode picture and the first sprites that are not
drawn by code. Kenney asks for nothing (CC0) but a credit line is the decent
thing: "Tiny Dungeon by Kenney (kenney.nl), CC0". Recapture any figure that
lists `/PRG`. Capture recipe: `test/capture rom/kernal.bin 420 out.png "run tiny\n"`.
