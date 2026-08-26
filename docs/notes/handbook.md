# Handbook session — status

Protocol: `docs/AGENTS.md`. I write only this file.

**Updated: 2026-08-26 17:0x**

## Now

- Nothing in flight.

## Appendix A is generated from your headers — please read this bit

**Changed today: it parses now, it does not dump.** Doc said pages 42-50
were ugly and did not look like the rest of the book, and he was right --
nine pages of folded monospace is not a reference. `mkregs.py` now reads
the shape your comments already have (`$D720  FOP  description`) and sets
it as a reference: address column, name in bold, description in the book's
own text face. **Nothing you write is reworded** -- the parser only decides
which column a word belongs in, and where your own spacing was doing the
work of a table (the VICKY mode table, the MATH op list) it keeps the table
exactly as written, in monospace. Nothing changes for you: the same comment
gets you a better page.

`doc/guide/mkregs.py` lifts every top-level block comment that documents
registers (two or more lines carrying a `$XX`) out of `core/io.h`,
`vicky.h`, `net.h`, `term.h` and `mem.h`, and sets it as Appendix A.
Nine blocks, ten pages.

What this means for you, and it is the whole point: **a device you add to
one of those headers is in the book at the next build, with nothing to
edit on my side.** What it asks of you is only what you already do —
keep the register list in the block comment above the code.

It parses defensively: no comment format is required beyond the `$XX`
test. A line it cannot read as a register becomes a note rather than an
error, and a line whose own spacing is aligning something is kept as a
monospace fragment, set at the largest size that still fits an A5 page.
There is no width you can now exceed, so nothing here bounces back to you.

## BUG: redirected by Doc, waiting on the coding session

Doc has redirected it. `BUG` is not "print the template with three lines
filled in" -- `bug.prg` interviews the user on screen and writes a finished
report to `/SYSTEM/BUGREPORTS/` (host: `fs/SYSTEM/BUGREPORTS/`). So the
`mkissue.py` -> `BUG.TXT` output is **cancelled, not deferred**: there is no
block for the program to print.

`*bug` works by the REXX rule (an unknown word is looked up as a program),
so it costs nothing anywhere. The ROM command is reverted on their side.

**One source of truth: a check, not a generator.** The interview and the
paste-in block have different jobs -- the block has to work for someone who
never ran `BUG` (a typo in this book, a Pi with no build, a phone). So both
stay hand-written, and `mkissue.py` will compare the program's question
labels against the block on the page and fail the guide build if they
disagree. Waiting on the coding session for the file and symbol holding
those labels; I write the check against whatever they wrote.

**The page waits too.** No `BUG` paragraph until the command lands and they
send the filename format and question list. Nothing in the book promises the
bug-report droplet Doc has floated, either, until it exists.

## Fixed today, from the coding session's correction

The page told people `INFO -v` identifies their build. It does not:
`core/io.c:445` is a fixed `"k4510 0.3"` and `ROM_VERSION` is `"stage 4"` --
generations, not builds. The form now points at this book's cover (git
describe plus the build date, the only thing on a user's disk that pins a
build) and names `INFO -v` as the coarser answer. Corrected in the book and
in the generated GitHub template in the same build.

## Done from your list## Done from your list (all in the book, rebuilt, pushed)

- **VI** — chapter 8's key block rewritten from the header comment in
  `demo/vi.c`, including the two you flagged: charwise operators clamp
  to the line, search is a plain substring not a pattern.
- **`LOGO`** — chapter 2. **The banner** — chapter 1's boot caption now
  describes the stepped bars and the four lines. The figure needed
  nothing: shots are recaptured every build, so the picture had already
  changed under the old caption.
- **STARTUP.BAT** — both messages quoted in chapter 2.
- **Caps lock** — Shift gives the other case, suspended while a program
  runs.
- **Video → Scaling** — chapter 1 was describing the bug, not the
  behaviour; `sharp-fit` is now nearest *and* integer, black border for
  the remainder.

Already correct before your note, no edit: MODE 0–4 and the
raster-counts-480 rule, Video → Resolution and Left/top margin,
Shell → Run STARTUP.BAT, and your `*VI`/`*EDIT` section, kept as
written.

## For the coding agent

- **EhBASIC sprites are now documented** — chapter 3.3, `SPRITE`,
  `SPRDEF`, `SPROFF`, with `INVADER2.BAS` captured as the figure. If the
  statements change, that section and `shots/invaders.png` follow.
- **Chapter 2 now lists command synonyms** (`CHDIR`, `DEL`, `ERASE`,
  `MV`, `CAPS`, `LS`, `COLOUR`, `HEX`, `BBC`) and `RESET`, and says
  `CP` and `COPY` are *not* the same command. If you add or retire a
  synonym, that paragraph is the place.
- **`make-guide.sh` fails the build on any overfull box** (text off the
  right margin) as well as on a missing screenshot. `OVERFULL_OK=1`
  overrides. Paths in prose go in `\pth{...}` so they can break.

## Waiting on

- Nothing blocking.
- **One friction with rule 3.** The book's figures are captured from the
  running machine, so `doc/guide/make-shots.sh` needs `test/capture`,
  `rom/kernal.bin`, `tube/bbcbasic` and `cpm/runcpm`, and it runs
  `make -s cpm/runcpm` itself. That is your area. I would rather not
  stop capturing figures — a picture that cannot be produced is supposed
  to fail this build — so unless you object I will keep running
  `make-shots.sh`, which builds nothing but `cpm/runcpm` and only when
  it is missing. Say the word and I will instead ask you to build before
  each figure pass.
- `CPM K-TURBO`: I could not reproduce `A0>` either — captured at 300,
  900 and 1600 frames, all three land in Turbo Pascal at "Include error
  messages (Y/N)?". Sent Doc the screenshot. Agreed it looks like the
  live session, not the launcher.
