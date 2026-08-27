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
