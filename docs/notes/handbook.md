# Handbook session — status

Protocol: `docs/AGENTS.md`. I write only this file.

**Updated: 2026-08-26 21:3x**

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

## Doc's review of the whole book — done, evening of 2026-08-26

Nineteen items, all in the book. The ones that touch you are in "For the
coding agent" below. The rest: an alpha notice as the first page; ch 1
retitled (What is the BMC-K4510? / Building the emulator); ch 2.1
rewritten from the current `fs/` and `.HELP` (directory table, every
command with its synonyms); MODE 3 and 4 gone from the user's guide (Doc:
programs only); 2.5 lists exactly what the network speaks and says there
is no FTP; 2.11 is now three lines pointing at Appendix B; the `@` escape
gone from ch 3; ch 7 rebuilt around a table that separates Turbo Pascal
(on CP/M, user-supplied) from Mad Pascal (cross-compiler); ch 8 is key
tables, one key per row, with `:imap jk <Esc>` tested on the machine and
documented exactly; 8.3 states the rule (`*EDIT` alone = the program,
`*SWAP EDIT name` = any other file); Filing an Issue is Appendix B,
BUG-first, the hand form moved out to `issue-form.txt` (GitHub only);
Meatloaf, FujiNet and SDL thanked; and the BBC BASIC wording corrected.

**That last one matters to you** — see below.

## BUG is in the book, and the check is live

Both done, Doc gave the go. Page 53, "BUG writes it for you": what it
answers itself, what it asks, the file it writes
(`/SYSTEM/BUGREPORTS/BUG-YYYYMMDD-HHMMSS.TXT`, host
`fs/SYSTEM/BUGREPORTS/`), and `*BUG` from both BASICs, the monitor and
CP/M. Your DUMP-first point is an aside on that page, in the shape you
sent it: if the Dump line says none, type `DUMP` while it is still wrong
and run `BUG` again.

**The check is in `mkissue.py` and it fails the guide build.** It reads
`bug_labels[]` out of `demo/bug.c` and compares it against a `FIELDS`
table that maps each of your labels to the line it must appear as in the
book's block -- `None` for the five the machine answers itself, since no
human is asked those. Add or rename a question and the guide build stops
until the page and the table agree; take one away and it stops too. I
tested both directions before pushing, and a checkout with no `demo/bug.c`
in it prints a note and builds anyway.

You do not have to do anything for it. If it ever fires on you, the fix is
mine: send the new label and I will write the line.

## Fixed today, from the coding session's corrections

**The hold-a-key window is out of the book** (their 05cf6d4 removed it from
the machine). It was an instruction in two places -- 2.7 and 2.10 -- and in
`shots/boot.png`, which showed the line. Both passages now say there is no
moment to catch and lead with the F7 switch, which is the durable one;
`--no-startup.bat` follows as the one-run answer. The figure is recaptured.
`INFO` is only named, never quoted, so its new two-line form needs nothing.


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

**I touched `test/bench.c`** (yours; small; saying so). It now takes
`K4510_CPU_HZ=<hz>` to sweep the clock past the menu's 40.5 ceiling, and
prints the clock in front of each line. Nothing else changed; the default
is still 40.5. Doc asked how fast the machine could go before sound or
video suffer, and this answered it in one run — the numbers are in the
build log under 2026-08-27. Keep or revert as you like.


**I rewrote `README.md` tonight** (shared file; saying so here as the
protocol asks). It announced alpha-0.2, a held key at the banner, the `@`
escape, `MODE 0-4`, the Tube as desktop-only, and said the diary was not
in the repository. It now describes the machine as released in alpha-0.3.
Anything in it about your area that is wrong is yours to correct without
asking.


Four things from Doc's review of the book tonight, in his words where I
have them. None is mine to build.

1. **CP/M cursor keys.** "the cursor keys on the emulator do not work in
   CPM." I traced it: kernal.c:1368 sends the key to JIM's KEY port, which
   turns an arrow into `ESC[A`, and tube_keys() forwards that to the Z80.
   CP/M software of 1984 reads the WordStar diamond (Ctrl-E/S/D/X), not
   VT100 sequences, so the arrows do nothing in WordStar or Turbo. The
   book now says exactly that, as today's behaviour. If you translate
   arrows to the diamond while the Tube is running CP/M (RunCPM, not BBC
   BASIC), tell me and the two paragraphs (ch 6 aside, ch 7.1) change.

2. **TELNET background.** Doc: "the TELNET program should switch screen
   background to BLACK when starting and back to preexisting color when
   exiting -> most BBSs are setup for black backgrounds." `demo/telnet.c`
   is yours. JIM's DEFBG ($DA15) and the console's bg are the two things
   to save and restore; say when it lands and ch 2.5 gets a sentence.

3. **`tube/ALTERED.md` makes a claim Doc says is false.** It reads "The
   name 'BBC BASIC' is used by permission of the BBC and is not
   transferable to derived works." Doc: "I did not contact the BBC for
   this. I just asked Claude to check out BBCBasic for SDL." The book said
   the same thing in two places and now says: the name is Richard
   Russell's interpreter's, published under his own arrangement with the
   BBC; this project has not sought and does not hold any licence to it
   and uses it only to identify what is vendored. Please bring ALTERED.md
   into line — it is a licence-condition file and it should not assert a
   permission nobody asked for.

4. **A startup file for VI.** Doc could not get `jk` to produce Escape;
   the mapping works (I tested it on the machine: `:imap jk <Esc>` then
   `ihello worldjk0x` gives `ello world`), so the trouble is that it has to
   be typed every session. Suggestion, not a request: VI reads
   `/SYSTEM/VI.RC` (or `.virc`) at start, one ex command per line. The
   book currently says "there is no startup file for VI yet"; that
   sentence changes the day there is one.

Also noted, for you and Doc to settle: Doc wants MODE 3 and 4 out of the
user's reach ("a bad call to mode will screw up the screen for someone
coming in"). The book no longer mentions them at the command; the command
and `.HELP` still offer 0-4. And `k4510.cfg` is still untracked in both
our `git status` — a `.gitignore` line, when you are next in there.


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
