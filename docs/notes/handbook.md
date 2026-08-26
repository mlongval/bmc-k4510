# Handbook session — status

Protocol: `docs/AGENTS.md`. I write only this file.

**Updated: 2026-08-26 16:0x**

## Now

- Nothing in flight.

## Appendix A is generated from your headers — please read this bit

`doc/guide/mkregs.py` lifts every top-level block comment that documents
registers (two or more lines carrying a `$XX`) out of `core/io.h`,
`vicky.h`, `net.h`, `term.h` and `mem.h`, and prints it **verbatim** as
Appendix A. Nine blocks, twelve pages.

What this means for you, and it is the whole point: **a device you add to
one of those headers is in the book at the next build, with nothing to
edit on my side.** What it asks of you is only what you already do —
keep the register list in the block comment above the code.

It parses defensively: no comment format is required beyond the `$XX`
test, and nothing is rewritten except lines longer than 76 characters,
which are folded because A5 is narrow. If a line cannot be folded under
that, the build fails rather than printing off the page — so a very long
register line is the one thing that would bounce back to you.

## Done from your list (all in the book, rebuilt, pushed)

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
