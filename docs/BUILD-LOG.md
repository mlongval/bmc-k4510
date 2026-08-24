# BMC-K4510 — Build Log

Dated record of what was actually done, as opposed to what was
planned. `K4510-Design.md` is the plan; this is the diary. Newest
entry last.

---

## Where things live

| What | Where |
|---|---|
| Design docs (this folder) | ubuntu-s1 `~/Projects/BMC64k4502/`, git |
| **Code** — bmc64 fork, branch `k4510` | **t480i5** `~/Projects/BMC64k4502/bmc64/` |
| Build | `./build-desktop.sh` in that repo |
| Run | `cd third_party/vice-3.3/data/K4510 && ../../src/xk4510 +sound` |
| Pi image builds (not started) | p15, when Phase 0's Pi half begins |
| Xemu (donor CPU core) | scratch clone; take `xemu/cpu65.{c,h}` fresh when Phase 2 starts |
| MEGA65 ROM for Xemu oracle | ubuntu-s1 `/media/doc/Internal_3TB/Emulation/Mega65/MEGA65.bin` |

**Workflow decision (2026-08-21):** develop on the desktop first, Pi
later. VICE 3.3 builds in ~2 min on the t480i5 and runs in an SDL
window on the machine Doc sits at, so there is nothing to copy. p15
is reserved for the Circle/ARM kernel-image builds, which are the
heavy ones. X forwarding over the tailnet was considered and rejected
for a 60 fps emulator.

---

## 2026-08-21 — Phase 0 (desktop half) and Phase 1 skeleton

### ASK-7: Xemu core audit — done, clean
See `ASK-7-xemu-audit.md`. Callbacks throughout; three hypervisor
sites; 6502-style reset. Phase 2 is a wrapper, not a port.

### Phase 0, desktop half — done
bmc64 `v5.0.2`'s VICE 3.3 builds on Fedora 43 (GCC 15) and `x64sc`
boots to `READY.`. What a 2018 tree needs on a 2026 compiler:

- **`./autogen.sh` first** — checked-in `aclocal.m4` is older than
  `configure.ac`, so a bare `make` tries to regenerate and fails.
- **`-fcommon`** plus `-Wno-error=incompatible-pointer-types`,
  `-Wno-error=implicit-function-declaration`,
  `-Wno-error=int-conversion`. All captured in `build-desktop.sh`.
- **One real source fix** (commit `e4e18301`):
  `arch/gtk3/archdep_unix.h` *defined* `const char *archdep_pref_path`
  instead of declaring it; the SDL build includes a gtk3 `.c` that
  pulls it in, and `const` defeats `-fcommon`. → `extern`.
- `vsid` and `c1541` fail on genuine tree bugs. Not built, not wanted.
- Build time: 1m46s, 8 threads.

### Phase 1 skeleton — done (commit `027fa0d6`)
`xk4510` exists and boots the C64 ROM.

- `src/k4510/` = the 60-file `libc64sc` set copied verbatim, names
  unchanged for now. `machine_name = "K4510"`, own log channel.
- **Deliberate crutch:** `machine_class` stays `VICE_MACHINE_C64SC`.
  Thirteen shared files switch on it (VIC-II resources, SID, CIAs,
  joystick, autostart…); keeping the value means they treat the new
  machine as a C64 and it boots with zero shared-code changes.
  `VICE_MACHINE_K4510 = 14` is reserved in `machine.h` for when the
  machine has hardware of its own.
- `data/K4510/`: kernal/basic/chargen + SDL keymaps/palettes.
  ROMs are found via `sysfile_init(machine_name)` → `data/<name>/`,
  so the directory name matters.
- Registered in `configure.proto`, `src/Makefile.am` (an `xk4510`
  target modelled line-for-line on `x64sc`), `data/Makefile.am`.

### VICE-isms learned the hard way
- **`configure.ac` is generated.** `autogen.sh` rebuilds it from
  **`configure.proto`**. Edits to `configure.ac` silently vanish.
- **`make -j` has a race** in the drive-library subdirs: two recursive
  makes compile the same `.o`, and `ar` bus-errored once. Rerun.
- ROMs are looked up relative to **CWD** and the data dir; the
  `-directory` option is the fsdevice path, not the ROM path.
- `-limitcycles N -exitscreenshot file.png` with
  `SDL_VIDEODRIVER=dummy` is the headless boot test. Exit code is 1
  on success (cycle limit reached), not 0.

### Launching onto the t480i5 desktop from ubuntu-s1 over SSH
```
cd .../data/K4510 && XDG_RUNTIME_DIR=/run/user/1000 \
  WAYLAND_DISPLAY=wayland-0 SDL_VIDEODRIVER=wayland \
  nohup ../../src/xk4510 -default +sound &
```
XWayland route needs `XAUTHORITY=/run/user/1000/.mutter-Xwaylandauth.*`;
native Wayland is simpler.

### Open bug: reSID segfault with sound enabled
`reSID::SID::clock_resample` at `sid.cc:1039`, 32768-sample buffer,
both under `SDL_AUDIODRIVER=dummy` and on the real desktop. Unrelated
to the machine work; `+sound` sidesteps it. Must be fixed before
anyone hears a SID. Suspect: the `-fcommon`-era merging of an
uninitialised global (a VIC-II log line is also tagged "Sampler
Filedrv", which is the same symptom — two `log_t`s merged).

### Not yet done
- **The strip.** Nothing has been removed from `src/k4510/` yet.
- Phase 0's Pi half (stock `kernel8-32.img` on the 3B+, benchmark).

---

## Next baby step

**Make `xk4510` boot with the cartridge system gone.** Not the whole
not-carried-in list — just cartridges, because it is the largest
inheritance, the one wired deepest into `c64memsc.c`, and removing it
forces the first real Phase 3 decision (what a read/write of `$xxxx`
does on this machine) while the C64 ROM is still there to say whether
it broke.

1. `c64memsc.c`: remove cart hooks (`cart_*`, roml/romh, Ultimax,
   I/O1–2 cart decode) and the RAM hacks (`plus60k`, `plus256k`,
   `c64_256k`, `c64-memory-hacks`). Delete those files from
   `libk4510`.
2. Drop `libc64cartsystem`/`libc64cart`/`libc64commoncart` from
   `xk4510_libs`. The linker's complaint list *is* the audit.
3. Stub or delete until it links. DigiMAX and the FM chip are
   implemented *as* carts — lift out, don't delete (E-01).
4. Boot. `READY.` with no cartridge code in the binary is the exit.

Tape, TDE, disk images, RS232, printer, MIDI come out in one sweep
afterward — they are shallow. Cartridges are the only deep one.

---

## 2026-08-21 (late) — Pivot: own core, VICE dropped

### The question
Half-way into the cartridge strip, Doc asked what of VICE we are
actually keeping and why we are going through it at all. Honest
accounting: as an *emulator*, two libraries (reSID, fmopl). Everything
VICE emulates — CPU, memory, video, carts, drives — we had already
decided to replace. What remained was VICE as a *framework*, and the
strip was showing what that costs: C64 assumptions in every file,
`viciisc` with cartridge hooks, SDL menus hard-linking cart code.

### The finding that settled it
The assumed reason for staying — "BMC64's Pi layer is a Circle port
of VICE, so we need VICE to get the Pi" — is **false**. BMC64 already
hosts a non-VICE emulator (plus4emu, `Makefile-Plus4Emu`) through a
defined interface:
- `third_party/common/emux_api.h` — 78 `emux_*` functions a core
  provides (attach, reset, joystick, frame buffer…)
- `third_party/common/circle.h` — 62 `circle_*` functions the Pi
  layer provides (framebuffer, audio, input, SD/FAT)
- `third_party/common/` ≈ 11k lines of Pi-hardened plumbing: menus,
  virtual keyboard, joystick config, CRT shaders, GPIO.
BMC64's Pi layer is emulator-agnostic. VICE is one tenant; plus4emu
is another; we will be the third.

### New shape
- `k4510core/` — Xemu `cpu65.c` (unchanged), our memory, VICKe,
  reSID + fmopl as libraries. Plain C, no VICE headers.
- Desktop frontend: SDL2 directly, a Makefile, no autotools.
- Pi frontend: implement `emux_api.h` against BMC64's common layer,
  as plus4emu does (its glue is 9 lines + the `emux_*` bodies).

### What is lost
VICE's monitor (23k lines; did not know the 45GS02 anyway — Xemu's
`emutools_umon.c` does), VICE's snapshot/resources framework (we
write small ones), and the `xk4510` skeleton from earlier tonight
(kept on the `k4510` branch as history; sunk cost).

### Decision gate: the spike
Nothing else until this runs:
**`cpu65.c` + 64 KB RAM + one VICKe text layer in an SDL window +
Wozmon in ROM + typing works. No VICE anywhere.**
If it is a few hundred lines and runs, the pivot is confirmed by a
program rather than an argument. If it is a swamp, we learn that
cheaply. The bmc64 `k4510` branch stays as the fallback.

---

## 2026-08-22 (overnight) — The spike ran. Pivot confirmed.

**Gate:** `cpu65.c` + 64 KB RAM + one VICKe text layer in an SDL window
+ Wozmon in ROM + typing works. No VICE anywhere.
**Result: passed, all four steps, in one night.** 961 lines of ours
around 3,000 of Xemu's. Screenshot: `images/spike-wozmon-2026-08-22.png`.

### Where the code is now
**t480i5 `~/Projects/BMC64k4502/k4510/`** — a new git repo, 4 commits.
The bmc64 fork (`k4510` branch) is now history only.

```
k4510/
  core/xemu/    cpu65.c cpu65.h timings, disasm tables  <- Xemu, byte-for-byte
  core/xemu/emutools_basicdefs.h   the shim: 60 lines, the core's whole environment
  core/hypervisor.h                in_hypervisor = false (constant)
  core/mem.c/.h                    64 KB RAM, the 11 callbacks, ROM protect, keyboard regs
  core/vicke.c/.h                  one 80x60 text layer, 8x8 glyphs, ASCII indices
  sdl/main.c                       SDL2 window, 40.5 MHz/60 Hz, keyboard -> FIFO
  rom/wozmon.a                     Wozmon retargeted; ACME --cpu m65; 4 KB at $F000
  test/cputest.c  test/woztest.c   both ALL OK
  data/chargen                     VICE's C64 font, reordered to ASCII at load
  Makefile
```
`make test` runs both tests. `./sdl/k4510` from the repo root runs it.
**Shift+Esc quits, F12 resets**, Backspace = Wozmon `_`, input is folded
to uppercase (1976 monitor).

### Step 1 — CPU (commit 187ecc1)
- The core compiles unchanged against the shim. Needed `Uint64` too.
- `flat_address()` for `[$nn],Z` / Q forms must **fetch its own operand
  byte** (`cpu65_read_callback(cpu65.pc++)`) — the core has not consumed
  it. I got this wrong first and caught it by reading Xemu's
  `memory_mapper.c:1014` before running. Mirrors upstream exactly now.
- `cpu_mega65_opcodes = 1` enables the extensions.
- Test: 6502 counter, 65CE02 `LDZ`/`INW`, 45GS02 `LDA [$20],Z` + `LDQ`/`STQ`.
  **The 32-bit flat pointer and Q ops passed on first run.** One test
  bug of mine (`$9B` is `STX $nnnn,Y`, 3 bytes, not a NOP).

### Step 2 — pixels (commit 38563c9)
- `vicke_render()` reads screen RAM at `$0800` every frame into an 8-bit
  framebuffer; the frontend owns palette and window. Two colours, C64
  blues, for now.
- Font: ASCII-ordered 8x8 built at load from `chargen` (uppercase set).
  PETSCII has no backslash, so `\` shows as `£`. Own font later.

### Steps 3+4 — Wozmon, typing (commit 0505f40)
- **ACME:** Fedora has none; the GitHub mirror is 0.96 (2019) and lacks
  `m65`. Built **0.97 from SourceForge svn** into `~/.local/bin/acme`
  (`svn export https://svn.code.sf.net/p/acme-crossass/code-0/trunk`).
  It emits exactly the bytes hand-assembled in step 1.
- Woz's logic is verbatim (source: jefftranter/6502 wozmon.s). Only
  `ECHO` (now a screen-RAM terminal with scroll) and the keyboard
  register are ours. `ECHO` **must preserve A, X, Y** — callers depend
  on it; PHA/PHX/PHY.
- Spike I/O map: ROM `$F000-$FFFF` write-protected; `$D010` key|`$80`,
  `$D011` bit7 ready (Apple-1 shaped because Wozmon polls it that way).
  **This is scaffolding, not the K4510 I/O map.**
- Authentic Wozmon quirks seen and kept: prints `\` on reset; a
  `0300:41 42 43` store line first prints `0300: 00` (the XAM before
  the `:` switches mode).
- `woztest`: boots, `FF00.FF0F`, store, read back, `400R` runs planted
  code (`$0700` = `$99` afterwards). ALL OK.

### What this settles
The VICE route is dead, by demonstration. Phase 0, Phase 1 (skeleton)
and the Phase 2 wrapper are done in the new shape. The Plan half of
`K4510-Design.md` needs rewriting around this repo; the capability
matrix stands.

### Still scaffolding (to be replaced, not extended)
- 64 KB flat RAM, MAP ignored → Phase 3: 256 MB + 28-bit MAP.
- Apple-1-shaped keyboard regs → the real K4510 I/O map.
- C64 chargen → own font, lowercase, backslash.
- Two-colour VICKe with fixed geometry → register block, colour RAM.
- Uppercase fold in the frontend → the ROM's problem, not SDL's.

### Phase 3 spine, same night (commit d2b59d9)
**256 MB and the 45GS02 MAP, in `core/mem.c`. `test/maptest` ALL OK.**
- Physical RAM is one `mmap(MAP_NORESERVE)` of 256 MB. Reserving it
  moved host RSS by 64 KB; after the tests touched pages across the
  whole range, by ~100 KB. Lazy commit is free from the kernel.
- MAP semantics taken from Xemu's `memory_mapper.c`, not a datasheet:
  `X==$0F`/`Z==$0F` select the megabyte per half; otherwise A/X, Y/Z
  give a 20-bit offset and a per-8KB-block mask;
  `phys = mb + ((offset + cpu_addr) & $FFFFF)`. I/O and ROM exist only
  in the unmapped view (as on the C65).
- Verified: block 1 → `$40000`; megabyte 5 into zero page; `STA/LDA
  [ptr],Z` at `$0FFFFFFF`; `STQ` at 8 MB.
- **Gotcha recorded:** the mask nibble is one bit per 8 KB block, and
  MAPping block 0 unmaps code at `$1000`. Test programs live at `$C000`.
- Wozmon still runs unchanged on top of it (`woztest` passes).

### After the spike, same night (commits 1c4ddac, e491dc7)
- **Own font.** `data/mkfont.py` builds `data/font8.bin` (ASCII-ordered,
  256×8) from the chargen's *second* set, which has lowercase; `\ ` { | } ~`
  drawn by hand. Wozmon's reset `\` is a backslash now.
- **`core/io.c`** owns the `$D000` page: dispatch by `$100` device page,
  keyboard FIFO at **`$D100`** (moved from the Apple-1 `$D010`; Wozmon ROM
  follows), and **block DMA at `$D200`** — SRC/DST/LEN 28-bit LE, CMD 1
  copy (memmove), 2 fill, 3 swap, instant. `test/dmatest`: 64 KB from
  1 MB to 255 MB, screen fill, overlap-safe scroll, bad-cmd status. ALL OK.
- **The I/O map in `io.h` is the PLAN-v2 proposal and is PROVISIONAL.**
  Every base is one constant; moving a device is a one-line change.
- `make test` = 4 tests, all green. Six commits on the new repo.
- Safety net: bare mirror at ubuntu-s1 `~/LocalRepositories/k4510.git`
  (remote `ubuntu-s1` on the laptop; `git push ubuntu-s1`). GitHub not
  pushed — waiting for Doc.

## Morning summary, 2026-08-22

What exists now: **a K4510 that boots its own ROM on a 45GS02 with 256 MB,
MAP, DMA and a text display, in 1,300 lines of ours + Xemu's core, with
four headless tests.** The window on the t480i5 is running it. Type hex
at Wozmon: `F000.F01F` dumps the ROM; `0300:41 42 43` stores; `300.302`
reads back; `400R` runs. Shift+Esc quits, F12 resets.

Decisions waiting for you, in order of how much they block:
1. **The `$D000` I/O map** (`PLAN-v2.md` §3, `core/io.h`). Agree, amend?
2. **`PLAN-v2.md`** itself — edit it, then it replaces the Plan half of
   the design doc.
3. Push `k4510` to GitHub (private)? Name: `mlongval/bmc-k4510`?

Next baby step, my pick: **VICKe palette + 8 bpp bitmap layer with a
register block at `$D000`** (VICKE-SPEC §13 step 1). That retires the
spike's fixed text layer as the only display and starts the real chip.

---

## 2026-08-22 — I/O map agreed, plan merged, GitHub, circle-libsdl2

Doc: I/O map stands as proposed; merge the plan; push.

- **I/O map agreed.** `core/io.h` is authoritative; "PROVISIONAL"
  notes there are now wrong and removed in the next code commit.
- **Plan merged** into `K4510-Design.md` as the Plan half; the VICE-route
  plan moved to `sources-superseded/PLAN-v1-vice-route.md`. `PLAN-v2.md`
  deleted. PDF/EPUB rebuilt.
- **circle-libsdl2** (github.com/Xalior/circle-libsdl2, found by Doc):
  from-scratch SDL2 for bare-metal Circle, Pi 3/4/5 AArch64, zlib,
  proven by a MAME port. Implements exactly what `sdl/main.c` uses
  (software renderer, streaming ARGB8888, scaled RenderCopy, HID,
  audio callback, RWops). **Phase 6 Route A is now "recompile our own
  frontend"**; BMC64 `emux_api` becomes Route B, the fallback. Risk is
  one author / no community, not the code. Toolchain: Arm GNU
  `aarch64-none-elf`, on p15 when the time comes.

## 2026-08-22 night 2 — VICKe, step by step

### VICKe step 1 (commit 0105db3) — palette + bitmap + text behind registers
`core/vicke.c` is a chip now: 256-byte register file at `$D000` (see
`vicke.h` header comment for the map), palette write port with
auto-increment, four 16-byte layer groups, per-scanline composition.
Bitmap 1/2/4/8 bpp; tile/text 8x8 1-bpp from a glyph set in RAM. No
video memory: font at phys `$010000` (frontend places it until the
system ROM carries it), ROM `VINIT` programs the chip at reset. The
C-side text path is gone — Wozmon is displayed through the registers.
`test/vicketest` ALL OK. 5 tests green. Pushed to GitHub.

### VICKe step 3 — tiles and text32 (pushed)
Tile mode: 8–64 px cells, 2-byte map entries (10-bit index, H/V flip,
4-bit palette offset), at the layer's bpp — 8 bpp tiles are the
full-colour tiles the spec wants. `text8` (1 byte/cell, Wozmon) and
`text32` (glyph16 + reverse + fg + bg bytes), both 8x8 or 8x16. Tested.

### VICKe step 5 — sprites (pushed)
128 sprites from a 16-byte-entry table in RAM (`$D00A` pointer): signed
X/Y, 28-bit data, 4/8 bpp, H/V flip, width and height 8–64 chosen
independently, Z-slot (drawn after layer 0–3). Sprite-sprite and
sprite-layer collision bitmaps at `$40`/`$50`, clear-on-read. Tested.

### VICKe step 6 — copper and interrupts (pushed)
Copper list in RAM (`$D060` pointer): END/WAIT/MOVE/SKIP/JUMP/IRQ, run
at each scanline start, restarts per frame. IRQSTAT/IRQMASK at `$04/05`
with vblank, raster-compare, copper, collision; ack by writing 1s. The
frontend now steps the CPU **per scanline** and drives `cpu65.irqLevel`
from the chip — raster IRQs work for real. Tested. Wozmon still runs.
- **The coprocessor is named SHEILA** (Doc). Renamed in code, spec and
  design doc; `VR_SHEILA`/`VR_SHEILACTL`/`VI_SHEILA`.

### VICKe step 7 — blitter (pushed)
`$D070-$D082`: SRC/DST 28-bit, W/H, strides, ops copy/keyed/fill/AND/
OR/XOR, H/V flip, instant, 8 bpp. Tested.

**VICKe status after night 2:** every item in VICKE-SPEC §13 steps 1–7
exists and is tested — palette, bitmap 1/2/4/8 bpp, tiles 8–64 px with
flips, text8/text32 at 8x8/8x16, 4 layers, 128 sprites with collisions,
SHEILA, blitter, IRQs, scanline-granular CPU. Not yet: active-area/
border registers, layer width/height clipping, per-layer Z register
(layers are in fixed order 0–3 with sprites interleaved by Z-slot),
sub-line SHEILA WAIT, 4-bpp blits, HAM/blend stretch items.
`make test` = 5 tests, all green. Repo: github.com/mlongval/bmc-k4510.

### Demo ROM and screenshots (pushed, d71d9e2)
`rom/demo.a`: SHEILA gradient, text, bouncing 8-bpp sprite — all from
45GS02 code. `./sdl/k4510 rom/demo.bin`. `test/capture ROM FRAMES OUT.png
[keys]` renders any ROM headless to PNG. Screenshots in `images/`:
`demo-2026-08-22.png`, `wozmon-2026-08-22.png`. Two bugs were in the
demo ROM, not the chip (Y-index wrap at 256 in the list builder; ASL
carry leaking into an address).
**Semantics change:** index 0 is transparent in every layer; BGCOL is
the ground (spec updated). The old rule hid SHEILA gradients under text.
The demo is running on the t480i5 desktop.

### Phase 4b — sound (pushed, dd66854, b8e1bf7)
`core/resid/`: reSID from VICE 3.3, vendored unchanged (C++, needs
`-DVERSION`). `core/sid.cc` wraps four chips at `$D400` (`$20` each);
`sid_render(cycles)` mixes them to 16-bit mono. SDL frontend: ring
buffer fed per scanline, drained by the audio callback at 48 kHz. The
demo plucks a triangle note on SID 0 at reset. `test/sidtest`: silence
is flat after the filter's reset transient; a sawtooth via the I/O page
has energy and oscillates. **6 tests green.** Not yet: OPL2 (fmopl),
PCM, stereo, model select per chip from software.

## Morning summary, 2026-08-22 (second night)

**What exists now:** a K4510 with every capability in VICKE-SPEC §13
steps 1–7, four SIDs, 256 MB + MAP, DMA, a Wozmon ROM and a demo ROM,
on GitHub with six headless tests. Lines of ours ≈ 2,600; vendored:
Xemu's core (3k) and reSID (3k). **The demo is running on the t480i5
desktop with sound**; `images/demo-sheila-2026-08-22.jpg` is what it
looks like.

Done tonight, in order: I/O map agreed → plan merged into the design
doc → GitHub → VICKe registers/palette/bitmap/text → tiles/text32 →
sprites → SHEILA (named by Doc) + IRQs + scanline CPU → blitter →
index-0-transparent fix → demo ROM + capture tool → reSID + audio.

Decisions/checks for Doc:
1. **Two spec deviations I made, flagged:** index 0 is always
   transparent (was "opaque in the bottom layer"); layers are in fixed
   Z order 0–3 with sprites interleaved by Z-slot (spec wanted a per-
   layer Z register — cheap to add, ask).
2. **Keyboard is still Wozmon-shaped** (`$D100` ASCII|$80 + ready bit).
   A real K4510 keyboard (scancodes, modifiers, key-up) is the next
   I/O item and wants a decision on layout (US-International policy).
3. OPL2 and PCM are the remaining sound items; straightforward.

Next baby step, my pick: **the system ROM, Stage 2** — the thing on
the critical path. A real terminal (text32 with colour, cursor,
lowercase), a keyboard driver, and a `LOAD` from the host filesystem
via `$D300` + DMA. That is the point at which the machine stops being
a demo and starts being a computer.

## 2026-08-22 — System ROM Stage 2 (pushed, f236923 + 767a124)

**The machine is a computer now.** `./sdl/k4510` boots `rom/kernal.bin`:
a colour terminal, a shell, and files from the host.

- **Toolchain:** Fedora's cc65 2.19 has no 45GS02 target, but its
  65C02 output (`BRA`, `STZ`, `PHX`, `(zp)`) is a strict subset, so the
  ROM is **C with cc65** (`--cpu 65c02 -t none`, own `k4510.cfg`,
  `crt0.s`). 5 KB of code in an 8 KB ROM at `$E000`. ACME stays for
  pure-asm ROMs (Wozmon, demo).
- **ROM:** `text32` terminal (colour, cursor blinks from the vblank IRQ,
  scroll by DMA), keyboard driver, shell with Wozmon's grammar
  (`addr`, `addr.addr`, `addr:b b`, `addrR`) + `LOAD name [addr]`,
  `SAVE name from.to`, `DIR`, `CLS`, `HELP`. Jump table at `$FF80`:
  CHROUT/CHRIN/GETIN/LOAD/SAVE.
- **Machine:** ROM up to 8 KB, top-aligned load. **Keyboard v2** at
  `$D100` (ASCII or `$80+` codes, modifiers in status; host layout does
  the dead keys on the desktop). **Host filesystem at `$D300`**:
  OPEN/READ/WRITE/CLOSE/DIR/STAT + LOAD/SAVE conveniences, sandboxed to
  `fs/` (second argv).
- **Lessons:** (1) cc65 C must never run inside an IRQ — it clobbers
  zero-page temporaries; the blink handler is assembly in `crt0.s`.
  (2) DMA copy is memmove-safe, so it does not replicate a seed row;
  `cls` copies row 0 to each row. (3) `$0800–$5300` is the text32
  screen; loading a file there paints its bytes as cell colours.
- **8 tests green**: cpu, woz, map, dma, vicke, sid, fs, rom.
- **Screenshots of all tests** in `images/`: `tests-2026-08-22.png`
  (console output), `test-*-2026-08-22.png` (one per VICKe stage and per
  ROM), `tests-visual-2026-08-22.jpg` (contact sheet),
  `rom-stage2-2026-08-22.jpg`.

Next: OPL2 + PCM; a per-layer Z register; the ROM's `LOAD` should set
the run address from a header; ASK-6/ASK-9 still open.

## 2026-08-22 (night) — Three demos, and the bug they found

Doc asked for three demo programs: 15 bouncing balls, a rotating
wireframe/solid cube (double buffered), and a Mandelbrot. All three are
C with cc65, in `k4510/demo/`, built as 8 KB ROMs (`make demos` →
`rom/balls.bin`, `rom/cube.bin`, `rom/mandel.bin`; run with
`./sdl/k4510 rom/balls.bin`). Screenshots: `images/demo-balls-*`,
`demo-cube-wire-*`, `demo-cube-solid-*`, `demo-mandel-*`,
`demo-mandel-zoom1-*` (PNG) and `demos-2026-08-22.jpg` (contact sheet).

- **balls** — 15 × 32×32 4-bpp sprites from **one** bitmap, 15 palette
  banks via per-sprite PALOFS; the sprite table is double buffered by
  flipping `SPRTAB`; SHEILA paints the sky gradient (16 MOVEs) and the
  floor. 640×480.
- **cube** — 320×240 in the **new lowres mode** (VICKe CTRL bit1: every
  pixel doubled; layers and sprites see 320×240, RASTER/SHEILA still
  count 480 lines). Two 4-bpp frame buffers at `$110000`/`$120000`,
  reached through the 45GS02 **MAP window** (`demo/crt0.s
  map_window()`: CPU `$2000-$BFFF` → any physical address; the
  megabyte MAP first, then offset+mask, then EOM). DMA fill clears, the
  flip is one write to the layer DATA pointer. 300 frames wireframe
  (all 12 edges), 300 frames solid with back-face culling, scanline
  fill and visible edges in white.
- **mandel** — 320×240 8 bpp at `$200000`, drawn row by row through a
  RAM buffer + DMA so you watch it. No multiplier on a 45GS02, so
  squares come from a 32 KB table of 4.12 squares built at start and
  MAPped in; `2xy = (x+y)² − x² − y²`. Cardioid and period-2 bulb tests
  skip the interior. ~16 s per image, then 3 s of palette cycling, then
  the next zoom level (5 levels into the seahorse valley).

**Bug found by the balls (real VICKe bug):** the collision registers
`COLSS $40-$4F` / `COLSL $50-$5F` sat exactly on **layer 3's register
block** (`$10 + 3×$10 = $40`). The first time two balls touched, the
collision bits "enabled" layer 3 with garbage settings and the screen
filled with junk. Moved to **`COLSS $90-$9F`, `COLSL $A0-$AF`**
(vicke.h, spec). Tests still 8/8.

Other lessons: (1) DMA fill takes its byte from the **SRC register's
low byte**, not from memory at SRC. (2) SHEILA instructions are 4 bytes
each, padded. (3) `test/capture` now prints the final PC/SP/IRQ state —
useful for "is it hung or is it rendering wrong". (4) cc65 demo RAM is
`$0200-$0FFF`; keep tables at `$1000+` or they eat the C stack.
(5) The sandbox image viewer I use mis-decodes capture's raw
stored-deflate PNGs; run them through `magick` first — I lost half an
hour chasing a "bug" that was the viewer, before finding the real one.

## 2026-08-22 (late) — ROM Stage 3: INFO, a bigger ROM, a system device

Doc: "an INFO command with unix-like flags, no flags dumps everything,
date/time too; add more functions as you see fit; 8 KB was only a
suggestion." So:

- **ROM is now 24 KB at `$A000-$FFFF`** (`rom/k4510.cfg`: ROM1
  `$A000-$CFFF`, the `$D000` I/O hole filled, ROM2 `$E000-$FFFF`).
  `mem_load_rom` sets a variable ROM base; I/O now wins over ROM in the
  write path so the hole works. ROM bss moved to `$5300-$5FFF`;
  **user programs get `$6000-$9FFF` (16 KB)** and `LOAD` defaults there.
  11 KB of the 24 used.
- **System device at `$D500`** (`core/io.c`): CPU clock kHz, RAM MB,
  host date/time latched on read of `$D504`, 24-bit frame counter,
  version string, ROM base page. SID registers `$00-$18` now read back
  their last written value (a shadow; real SIDs are write-only) so
  `INFO -s` can show volume/gates/filter.
- **`INFO [-v -c -m -g -s -f -t]`**: version, CPU (nominal + *measured*
  clock: `speed_loop` in crt0 counts an 18-cycle loop for one frame →
  40.42 MHz), memory map + last load, VICKe state (resolution, layers,
  sprites, SHEILA, raster, IRQ mask), the four SIDs, host files, date/
  time/uptime. No flags = everything. Screenshot
  `images/rom-stage3-info-2026-08-22.png`.
- **New commands:** `TYPE name`, `RUN [addr]` (defaults to the last
  LOAD), `FILL from.to value`, `COPY from.to dest`, `COLOR fg [bg]`,
  `ECHO`, `TIME`, `RESET`. Commands are case-insensitive.
- **Wozmon grammar is 28-bit now:** examine/store/block beyond 64 KB go
  through DMA (`peek`/`poke` copy one byte via `$D200`), so
  `1000000.100000F` and `1000004:55` work at 16 MB. Addresses print as
  8 hex digits.
- The banner prints the date and time at boot.
- romtest extended (FILL/TYPE/INFO/28-bit access); its user program
  moved from `$0700` (inside the C stack — it had been getting away
  with it) to `$5F00`. 8/8 green.

Next candidates: the `.prg` header + `RUN` for the demos as loadable
programs (the step I proposed); per-layer Z; OPL2.

## 2026-08-22 (morning) — Programs: LOAD, RUN, and back

The step I proposed: what a *program* is on this machine.

- **`.prg` format:** 4-byte header — load address, run address — then
  the image. `LOAD name.prg` honours it (`LOAD name.prg addr` overrides
  the load address); `RUN name.prg` loads and runs; `RUN` alone reruns
  the last one; `RUN addr` still works. The ROM re-initialises VICKe
  (including the first 16 palette entries) and clears the screen when
  the program returns.
- **Program environment** (`demo/prg.cfg`, `demo/prg0.s`): code at
  `$6000-$9FFF`, own zero page `$40-$63` and own C stack at the top of
  that range, so the ROM's state survives; the ROM's IRQ keeps running.
  vblank comes from the `$D50D` frame counter. Any key returns to the
  shell. `prg0.s` also provides `far_poke/far_poke16/far_peek` — the
  45GS02's NOP-prefixed `STA/LDA ($zp),Z` 32-bit flat forms, the first
  use of them in our own code — and a 16 KB MAP window at `$2000-$5FFF`
  (the text screen's RAM, idle while a program runs).
- **The three demos are now `fs/balls.prg`, `cube.prg`, `mandel.prg`**
  (3.5–6.7 KB). Their big tables moved to far memory: sprite tables and
  SHEILA list written with flat stores, the cube's 8-bpp frame buffers
  drawn with flat stores and DMA-fill spans, the Mandelbrot's square
  table halved by symmetry to fit the 16 KB window. Screenshots
  `images/prg-shell-2026-08-22.png`, `prg-balls-2026-08-22.png`.
- **Bug of the day:** `RUN balls.prg` jumped to `$00BA` — "b" and "a"
  are hex digits, so the name parsed as an address. `mandel.prg` had
  worked. A token is a name if it contains any non-hex character.
- ROM bss is back in block 0 (`$0200-$07FF`) so a program may MAP
  `$2000-$5FFF`; `$5300-$5FFF` is free. RODATA moved to the `$E000`
  half of the ROM — the code half was 20 bytes over 12 KB.
- romtest covers LOAD-with-header, RUN, and the return on a key. 8/8.

Next: OPL2 + PCM (`INFO -s` already says "not fitted yet"); per-layer
Z; ASK-6/ASK-9.

## 2026-08-22 — Portability review and the 21 ms surprise

Doc asked whether to optimise before the Pi. Answer: measure, don't
optimise — and the measurement paid off. `test/bench` (headless
frame-time split) showed **reSID taking 21.5 ms of a 16.7 ms frame** on
the i5: the four SIDs were clocked at the CPU's 40.5 MHz, 41× a real
SID. SDL had been quietly running behind real time since the sound went
in. SIDs now run at 1 MHz whatever the CPU clock (`SID_HZ` in
`core/sid.cc`); reSID fell to 0.63 ms, a frame is 3.4–4.1 ms total, and
the frequency registers mean what they mean on a C64.

The review itself is `PORTABILITY.md`: host dependencies live in two
files (`mem.c`: mmap + ROM loading; `io.c`: the storage device's POSIX
file calls and `time()`), everything else — cpu65, VICKe, reSID — is
clean; alignment/endianness/pointer-size checked; proposed `core/host.h`
seam (alloc, log, clock, eight file calls) as the first commit of the Pi
work. Pi 3B+ extrapolation: 17–33 ms single-core → CPU emulation and
VICKe+SID on separate cores is probably needed, and the per-line
interface already allows it.

p15 is reachable (16 threads, gcc/make/git, no cross toolchain yet) —
the Pi build goes there.

## 2026-08-22 — Pi: the SDL-layer probe (built, not yet booted)

Doc: before cross-compiling the emulator, a test kernel that only asks
"does the SDL2 layer on the Pi do what we need?" Done on p15:

- **Toolchain:** Arm GNU 15.2 `aarch64-none-elf` in p15 `~/opt/`
  (14.2 was too old: circle-stdlib's libc++ 22 needs GCC 15 builtins).
  Plus `cmake`, `ninja`, `texinfo`. The rpi3 "world" (Circle Step51 +
  newlib 4.5 + libc++ 22, multicore, 2 MB stacks) and `libSDL2-rpi3.a`
  are built: p15 `~/Projects/k4510-pi/circle-libsdl2/`.
  Gotcha: `make world` is idempotent on `Config.mk`; after changing the
  toolchain, `rm Config.mk` and `git checkout -- build install` (the
  placeholder dirs are tracked) before rebuilding.
- **`k4510probe`** (`pi/k4510probe/` here, sources + README): a Circle
  kernel that does exactly what `sdl/main.c` asks of SDL and measures
  it — 640×480 ARGB streaming texture refilled every frame from an 8-bit
  buffer through a palette (PRESENT ms on screen), a synthetic CPU load
  (2 M-iteration integer loop ≈ 1.9 ms on the i5, and a 256 KB memory
  walk) to calibrate the Pi/i5 ratio, USB keyboard scancodes, 48 kHz
  S16 mono audio callback (SPACE = 440 Hz tone), SD card read/write via
  `fopen`, a touched 256 MB `malloc`, CPU MHz and SoC temperature. Own
  3×5 font so the HDMI picture alone tells the story; everything also on
  serial. 980 KB `kernel8.img`.
- **`pi/k4510probe-sd-2026-08-22.tar.gz`**: the complete SD card —
  firmware, `config.txt` (arm_64bit), our kernel, `k4510probe.txt`, and
  `alt/` with the library's own gradient / keyecho / tone kernels as
  fallbacks. Untar onto a FAT32 card, boot the 3B+, read the numbers.

Not yet run on hardware. The numbers to bring back: FPS, PRESENT ms,
INT LOOP ms (÷1.9 = the slowdown factor), whether keys/tone/file work.

## 2026-08-22 — First light on the Pi 3B+ (and a 2 GB card lesson)

**Run 1 (probe without SD mount): screen, scaling, USB keyboard and HDMI
audio all work bare-metal** on Doc's BMC64 box (C64 keyboard on GPIO
attached and ignored; an 8BitDo USB keyboard receiver; a USB stick;
panel 1824×984). "Flabbergasted am I." No files, because the kernel
never mounted the card — the library leaves EMMC + `f_mount("SD:")` to
the host kernel, unlike everything else it brings up itself.

**Run 2 (with the mount): hung in boot messages.** Photo in
`screenshots/IMG_6002.jpeg`. The log shows why: `emmc: Capacity is 967
MBytes` for a 1.9 GB card, then `error sending CMD24 … Giving up`.
Circle's SD driver computes CSD-v1 capacity without `READ_BL_LEN`
(1024 on 2 GB SDSC cards) — half the size, and block writes fail. The
write-retry loop stalled the program before `SDL_Init`. **Use an SDHC
card (≥ 4 GB).** The probe now turns file logging off after the first
failed write and says so on screen, so a bad card still shows the
picture and the numbers.

## 2026-08-22 (evening) — The Pi numbers

Three more boots on the BMC64 box (Pi 3B+, Sharp 1080p TV, USB
keyboard). Photos `screenshots/IMG_6003.jpeg`, `IMG_6004.jpeg`.

**Run 3 (4 GB SDHC, TV at 1080p):** picture, sound, keys — and the
first real numbers. INT LOOP 2M = **8.74 ms** (i5: 1.9 → the Pi is
**4.6× slower**, better than the 5–8× guessed). MEM WALK 256K 0.97 ms.
But PRESENT = **22.8 ms**: the library scaling 640×480 onto 1824×984
(1080p minus the firmware's overscan margins) cost more than a frame;
29.8 fps. Files still failed — wrongly blamed on the card by my own
message.

**Run 4 (config.txt: `hdmi_group=1 hdmi_mode=1 disable_overscan=1`, the
TV driven at 640×480 and upscaling itself):** PRESENT **6.8 ms**, frame
**17.2 ms / 58 fps** *including* ~9.6 ms of synthetic load. Card: mount
0, open 0, create 0, 3781 MB, "valid version 3.0x SD card"; read and
write OK. So the 2 GB card really was the earlier problem, and the
reformatted 4 GB (ex-BMC64, backed up on p15) is fine. Per-second
logging stopped after the setup lines — one failed append and my code
gave up; now it retries and shows ok/failed counts on screen.

**What this means for the port:** the emulation core (3.4–4.1 ms on the
i5) should land around **16–19 ms on one Pi core** — marginal alone,
comfortable once CPU emulation and VICKe+SID run on separate cores
(the library's core split exists for exactly this). Presentation at
640×480 output is ~7 ms on core 0, or free to the application core
with the split's presentation worker. 256 MB allocates in 4 ms. The
display must be driven at a small mode (BMC64 used a custom 768×544
for the same reason). Keyboard: USB fine; a 2.4 GHz 8BitDo dongle
dropped SPACE/ESC (HID report quirk) — note for later, the real machine
uses the C64 keyboard on GPIO anyway.

Files: `pi/k4510log-run3.txt` (the setup block the card kept),
`pi/k4510probe/config.txt` (the working TV mode).

## 2026-08-22 (night) — THE K4510 BOOTS ON THE PI 3B+

Doc: "Pi frontend and C64 keyboard on GPIO it is." Same evening:

- **`core/host.h`** — the seam from PORTABILITY.md turned out to be two
  functions (`host_alloc_zeroed`, `host_zero`) plus a per-frame input
  hook. Everything else the core uses — `fopen`, `opendir`, `stat`,
  `time()` — newlib on Circle provides. `sdl/host_posix.c` keeps the
  mmap path; `pi/host_circle.cpp` uses the kernel heap.
- **`sdl/main.c` is the Pi frontend too**, unchanged but for its entry
  name (`k4510_frontend_main`) and the hook. circle-libsdl2 delivers
  `SDL_TEXTINPUT`, key repeat, the audio callback at our 48 kHz S16
  mono, and the 640×480 ARGB streaming texture exactly as desktop SDL
  does. Route A, as designed on 2026-08-21.
- **`pi/`**: Circle kernel (EMMC + `f_mount("SD:")`, `chdir SD:/k4510`,
  then the frontend with `rom/kernal.bin` and `fs/`), Makefile (Circle's
  Rules.mk compiles the C core; a `.cc` rule for reSID), `make-sd.sh`
  (firmware + kernel at the root, `/k4510/{rom,data,fs}`), `config.txt`.
- **`pi/c64kbd.cpp`**: the C64 keyboard on GPIO, wired the BMC64-PCB way
  (BMC64 "GPIO Config 2", which is what Doc's `settings.txt`
  `gpio_config=1` means). Rows GPIO 5,20,19,16,13,6,12,26 driven low in
  turn; columns 8,25,24,18,23,27,17,22 read with pull-ups; RESTORE on
  GPIO 4. Standard matrix → our `$D100` ASCII/KEY_* codes: shift gives
  the upper symbols, C= = Alt, CTRL = Ctrl, RUN/STOP and RESTORE =
  Escape, cursor keys shift into up/left, F-keys into the even ones;
  0.5 s then 30/s repeat. Read from the frame loop via the hook.
- **First boot stopped at `sdhost: unexpected command 13 error`** right
  after `SDL2Circle_ArmCoreRuntime`: that call raises the CPU clock, and
  the core clock the SD host's timing derives from moves with it. The
  library documents the cure — `SDL2Circle_HardwareInit()` in the kernel
  constructor, before the SD driver — and a retry on the first access.
- **Second boot: it works.** Blue shell, `DIR`, `HELP`, the C64
  keyboard typing (Doc's `di` → Wozmon examined `$D`, as it should),
  `RUN balls.prg`, `cube.prg`, `mandel.prg` on the Sharp TV. Photos
  `screenshots/IMG_6008..6011.jpeg`. Single core, no split yet.
- Linked first time, 1.26 MB `kernel8.img`. 8/8 desktop tests still
  green; desktop frame time unchanged.
- Card gotchas: a card pulled mid-write left a dirty FAT (`fsck.vfat`
  fixed it); a stale mount on p15 looked like a write-protected card.
  TV overscan crops 480p edges → `overscan_*` margins in `config.txt`.

Still to do on the Pi, in order: **measure** (`INFO -c` on the Pi is
the number), the **core split** (CPU on one core, VICKe+SID on another,
presentation on a third), a real-time clock (network or a saved
date), and the desktop's Shift+Esc quit becomes a reboot.

## 2026-08-22 (late) — Core split, full speed, the picture, the keyboard

- **Keyboard test** (`fs/keytest.prg`, through the ROM jump table — the
  first program to call CHROUT/GETIN): 64/71 with every miss a wrong
  key; all three modifiers and the shifted combinations correct. The
  GPIO matrix table is right. Report saved to `fs/keytest.txt` by the
  program itself. Two to recheck at leisure: ↑/← arrows, CTRL+A.
- **Core split** (`pi/kernel.cpp`): core 0 devices + servo, **core 1
  the emulator**, core 2 presentation, core 3 parked. `sdl/main.c`
  still unchanged; file calls from core 1 redirected to the library's
  I/O service by `circle-syscallwrap` (one include in the Makefile —
  which then became the default goal; `.DEFAULT_GOAL := kernel8.img`).
  **Result: `INFO -c` on the Pi = 40.42 MHz, 37,427 iterations — the
  desktop number exactly; 4655 frames in 1:17 = 60 fps.** The K4510
  runs at full speed on the Pi 3B+.
- **Picture:** Doc found the firmware overscan margins squash the image
  and 720p output resamples the font (640→960 is 1.5×, uneven). Final:
  **640×480 output, 1:1 present, no margins**; the TV scales. If a TV
  crops, its "Just Scan"/"Screen Fit" setting is the fix.
- **VICKe 640×240** (CTRL bit2, lines doubled) beside 320×240 (bit1).
  **ROM `MODE 0|1|2`**: 80×60 / 80×30 / 40×30 text; COLS/ROWS are
  variables now; `INFO -g` reports the mode. Photo of MODE 2 on the TV:
  `screenshots/IMG_6020.jpeg`.
- **Demo speed, measured** (new on-screen FPS meters; cube's D key
  toggles double buffering, mandel shows seconds per image): balls 60
  fps; **cube 15 fps on desktop and Pi alike** — it is bound by the
  emulated 45GS02 (a cc65 far-store per pixel, 32-bit edge math), not
  by the host, which is why the split moved balls and mandel but not
  the cube. Single-buffering cannot help it; it just tears.
  The honest fix is in the design, not the program: a **line-draw op in
  the blitter** (VICKE-SPEC §8 already allows "instant" blits) would
  make wireframes free, the way the span fill already is via DMA.
  Proposed, not done.

**Blitter LINE + TRIANGLE (same night).** Doc: "not a purist — if you
can make it faster, do." Ops 6 and 7, clipped; `vicketest` gained two
checks. Cube: **15 → 60 fps, wireframe and solid**. First try (lines
only) gave 60/22: the solid half was still bound by 450 DMA span
setups per frame, each with a 32-bit multiply in cc65 — the triangle
op removed the edge walker and the spans both. Lesson for every later
program: on this machine, geometry goes to VICKe, not to the CPU.

## 2026-08-22 (night) — The MATH unit

Doc asked whether the machine needs a "virtual FPU". Yes — the cheapest
device we will ever add. `$D700`, in `core/io.c`:

- **Float unit:** eight IEEE-single registers F0..F7 (`$D700-$D71F`),
  ops that work *in place* — write `FARG = dst<<4|src`, write `FOP`,
  done: MOV ADD SUB MUL DIV SQRT SIN COS TAN ATAN ATAN2 EXP LOG POW ABS
  NEG FLOOR ROUND FMOD, CMP (flags only), ITOF/FTOI through a 32-bit
  `FI` register. Two byte writes per operation is the whole point: on
  an 8-bit bus a naive "write 8 operand bytes, read 4" FPU loses to a
  table lookup.
- **Integer unit, MEGA65-compatible, same addresses:** `MULTINA/B` at
  `$D770/$D774`, 64-bit `MULTOUT` at `$D778`, 32.32 `DIVOUT` at
  `$D768` — so MEGA65 conventions (and a future BASIC) carry over.
- `test/mathtest`, 9/9 suites green.
- **Mandelbrot rewritten on it:** no table, no MAP window, no fixed
  point; eleven register ops per iteration. 64 iterations in 13 s vs
  40 in 16 s, and eight zoom levels where 4.12 fixed point gave out at
  four. Bug on the first run: loaded a constant into a register *after*
  multiplying by it — whole set "inside", all black, 3 s. Register
  discipline matters when the registers are in I/O space.
- On the Pi's A53 the float ops are hardware; on the desktop they are
  C `float`. The Pi kernel is rebuilt with the device and on the card.

Where this leads: BASIC with real floating point that is fast, and any
program's 3-D maths, at the cost of nothing.

**Math lists (same night).** Doc asked if a 16-bit FPU would be faster.
No: the arithmetic is free on the host; the cost is the 8-bit CPU
issuing ops, ~14 cycles each, and a narrower register saves nothing
per op. The fix is fewer issues: a **math list** — SHEILA for numbers.
`MLPTR` points at a program of the same 2-byte ops in RAM, `MLRUN`
runs it on the unit until `END` or a `STOP*`; `JUMP`/`DJNZ` (with a
16-bit `MLCNT`), `STOPFIGE` (stop when FI ≥ n), `LDF`/`LDI` immediates.
The Mandelbrot iteration is a 16-op list: per pixel the CPU writes
three bytes and reads two. **Under 1 s per image, from 16 s this
morning** — and the same mechanism is how a fast BASIC will evaluate
expressions. Bug: jump offsets are relative to the op *after* the
jump; my first lists were one short. `mathtest` covers a looping list.

## 2026-08-22/23 — EhBASIC stands up, with graphics

Doc: "Step 1: stand up EhBASIC. Step 2: add graphics commands."

**Groundwork, because BASIC owns the machine.** EhBASIC uses zero page
`$00-$E5` and `$EF-$FF`, page 3, and wants a contiguous program area.
So the ROM became a host: (1) its **text screen moved to far memory
(`$030000`)** — VICKe reads it there, the ROM writes it with flat
stores and DMA, the cursor blink in the IRQ borrows `$02-$05` and puts
them back — which frees **`$0800-$9FFF`, 38 KB, for programs**;
(2) every jump-table call **swaps the ROM's zero page (`$02-$21`) in
and out**, and `call_prog` snapshots it before a program runs and
restores it after; (3) the ROM's C stack sits at `$0500-$07FF` so a
program may use `$0300-$04FF`. Bug: `call_prog` patched its own JSR —
in ROM. Indirect jump through RAM instead. 9/9 still green.

**EhBASIC 2.22** (`basic/`: Lee Davison's `basic.asm` in ca65 form
from jefftranter/6502, `Ram_base $0800`, `Ram_top $7000`, code at
`$7000`; `k4510basic.asm` = the host glue: input/output through the
ROM jump table, CR-only newlines, a-z folded to upper case as a C64
does, RUN/STOP = reset to the shell; `README-EhBASIC.txt` carries the
"Derived from EhBASIC" notice). `fs/ehbasic.prg`, 10.9 KB, **26,623
bytes free**. `PRINT 2+2`, loops, `SQR`, `PI`, strings all correct.

**Graphics keywords** (`basic/k4510gfx.asm`, six new tokens spliced
into the command, keyword and LIST tables): `GRAPHICS n` (0 off,
1 = 320×240, 2 = 640×480; 8-bpp bitmap on VICKe layer 1 above the
text, index 0 transparent, cleared on entry), `GCLS`, `PLOT x,y,c`,
`LINE x1,y1,x2,y2,c`, `TRI x1,y1,x2,y2,x3,y3,c` — all blitter ops, so
instant — and `PALETTE i,r,g,b`. Arguments go through EhBASIC's own
expression evaluator. Note: `GRAPHICS 1` shows only 40 of the ROM's 80
text columns; `GRAPHICS 2` keeps the full text screen.

Not done yet: LOAD/SAVE of BASIC programs (vectors are stubs), the
MATH unit under EhBASIC's float package (that is the big win and the
reason the unit exists), `BYE`.

## 2026-08-23 (small hours) — LOAD/SAVE, a tune, BASIC demos, the package

- **`sids.prg`**: two SIDs — melody + fast arpeggio on SID 0, filtered
  bass + noise drums on SID 1, 16 bars in A minor at 120 bpm, level
  bars per voice. The first music on the machine.
- **EhBASIC LOAD/SAVE** (`basic/k4510file.asm`): `LOAD "NAME.BAS"` reads
  the text file into far memory with the ROM's LOAD and feeds it through
  the input vector as if typed (output muted, a leading CR for the
  Ctrl-C check to swallow); `SAVE "NAME.BAS"` captures LIST through the
  output vector and writes it with the ROM's SAVE. Plain text: a PC can
  write programs too. Input lines raised to 126 characters.
- **Three bugs, all memory layout, all mine:** (1) my page-3 variables
  sat on EhBASIC's input buffer (`$0321-$0368`) — `GRAPHICS 2` wrote 480
  into the line being parsed; (2) the ROM's BSS had grown (the 320-byte
  row buffer) from `$0227` to `$03E7`, over page 3 entirely — every
  printed line trampled EhBASIC; the ROM now has DATA `$0200-$02FF`,
  BSS `$0440-$05FF`, stack `$0600-$07FF`, and blanks rows from a far
  template; (3) EhBASIC's LIST formats numbers in `$EF-$FF` — the
  ROM's parameter block — so SAVE sets its parameters after LIST. Plus
  one non-bug: `RND(1)` in EhBASIC *reseeds*; `RND(0)` is "next".
  And a Makefile lesson: the `.prg` rule lacked the included files as
  prerequisites, so for an hour I was testing a stale binary.
- **Five BASIC demos** in `fs/`: README, LINES, TRIS, SINE, STARS.
- **`pi/bmc-k4510-pi3-2026-08-23.zip`** (2.3 MB): the complete card —
  firmware, kernel, ROM, demos, BASIC, a README — for Doc's friend.
  Unzip onto a FAT32 SDHC card, boot a Pi 3B+.

## 2026-08-23 — Public

Licence audit before opening the repo: Xemu core GPL-2.0-or-later,
reSID GPL-2.0-or-later, cc65 runtime zlib-style, Circle (GPL-3.0) and
circle-libsdl2 (zlib) outside the repo, EhBASIC under Lee Davison's
non-commercial terms as a separate program with his notice. **One
problem: `data/chargen` was a Commodore 64 character ROM**, in the tree
since the spike, with `font8.bin` derived from it. Replaced by the
Linux kernel's `font_8x8` (GPL-2.0, `data/mkfont.py` rebuilds it), the
file purged from the whole history (`git filter-branch`, gc, force-push
to GitHub and the mirror; zero references remain), the zip and the
card rebuilt with the new font. `LICENSE` (GPL-2.0-or-later),
`LICENSES.md`, README rewritten for strangers.
**github.com/mlongval/bmc-k4510 is public.**

**EhBASIC on the MATH unit (same night).** `basic/k4510math.asm`: the
Microsoft float in FAC1/FAC2 ↔ IEEE single is a byte shuffle (exponent
− 2, the 23 bits after the explicit leading 1, sign bit), so `LAB_ADD`,
`LAB_MULTIPLY`, `LAB_DIVIDE` and `SQR SIN COS TAN ATN EXP LOG` now
begin with a `JMP` into routines that convert, issue one op, convert
back; zero, negative and overflow keep EhBASIC's own errors. Every
check value matches; `BENCH.BAS` (5000 × `A=A+SQR(I)*1.5/3+SIN(I/100)`)
**4.03 s → 1.66 s**, identical result. What remains is the interpreter
walking text and variables — level 2 (expression → math list) is the
next multiplier, and a different kind of work.

## 2026-08-23 — Level 2: expressions become math lists

`basic/k4510expr.asm`, hooked at `LAB_EVEX`: in a program, a numeric
expression is compiled once — constants `LDF`, variables `LDMS` (a new
unit op that loads a Microsoft-format float straight from memory),
operators and SQR/SIN/COS/TAN/ATN/EXP/LOG/ABS/INT as register ops on a
seven-deep register stack, `^` left-associative as EhBASIC has it —
and cached by the expression's address (256-entry table + arena in far
memory at `$0D0000`). Later evaluations: one write to MLRUN, one
conversion back. Unknown things (strings, arrays, RND, PEEK,
comparisons, FN) bail to the interpreter, which still uses the unit
for arithmetic. Cache emptied at RUN/CLEAR/NEW and on line entry;
immediate mode never cached. Counters at PEEK 1051..1055.
**BENCH.BAS 4.03 → 1.66 → 0.53 s; BENCH2 (20000 pure-expression
loops) 6.46 → 2.11 s.** What is left is statement dispatch.
Bugs: PI pointed at the wrong constant; and for an hour every list
"failed" — I had rebuilt only `mathtest` after adding LDMS, so the
emulator under `capture` had no such op. Always `make all`.
EhBASIC moved to `$6C00` (12.7 KB now; 25,599 bytes free).
`GETIN` shows the cursor while waiting, so BASIC has one again.
Zip rebuilt; the Pi kernel rebuilt (new op) — card not in p15 tonight.

## 2026-08-23 — Benchmarks: CHROUT, and the classic 8-bit suite

Doc asked two things after the "programs bigger than 64 KB" discussion
(FEATURES.txt section K, K-01..K-04, written today): would an assembly
ROM be faster, and how does the machine compare on the period
benchmarks (github.com/rprouse/8bit-benchmarks: Rugg/Feldman 1977,
Byte Sieve 1981, Ahl's Creative Computing 1983). Measure first.

**New in `k4510/`** (on t480i5, uncommitted — Doc's call):
- `demo/chrout.c` → `fs/chrout.prg`: CHROUT throughput through `$FF80`,
  three passes (character stream / 79-char lines / bare newlines),
  timed by the `$D50D` frame counter so the host does not matter.
- `demo/sieve.c` → `fs/sieve.prg`: the Byte Sieve in C, 10 iterations.
- `fs/RF1.BAS`..`RF8.BAS`, `AHL.BAS`, `SIEVE.BAS`: the BASIC benchmarks
  verbatim plus a frame-counter wrapper (lines 1–2 and 750–770).
- `test/headless`: boot a ROM, type keys, run until a marker string is
  on the text screen (or `"a|b"`), dump the screen. General tool.
- `test/benchmarks.sh`: runs the lot headless (~1.5 min on the t480i5).
- Benchmark programs wait for a key after `DONE` — `run_at()` in the ROM
  does `cls()` when a program returns, which ate the first results.

**Results** (1 frame = 1/60 s, 45GS02 at 40.5 MHz, EhBASIC with the
MATH-unit floats and the expression compiler):

| Benchmark | K4510 | C64 / 1 MHz 6502, for scale |
|---|---|---|
| Rugg/Feldman 1 | 0.05 s | 1.2 s |
| RF 2 | 0.23 s | 9.3 s |
| RF 3 | 0.29 s | 17.6 s |
| RF 4 | 0.29 s | 19.5 s |
| RF 5 | 0.38 s | 21.0 s |
| RF 6 | 0.76 s | 29.5 s |
| RF 7 | 1.18 s | 47.5 s |
| RF 8 (100 × ^, LOG, SIN) | 0.03 s | 11.9 s |
| Ahl's (accuracy .0115, random 31.2) | 0.54 s | 1:53 |
| Byte Sieve, BASIC | **out of memory** | minutes (one iteration) |
| Byte Sieve, C (cc65), 10 iterations | 0.93 s | tens of seconds |
| CHROUT, character stream | 4,000 cycles/char, 10,100 ch/s | — |
| CHROUT, 79-char lines | 3,780 cycles/char, 10,700 ch/s | — |
| CHROUT, newline only (scroll by DMA) | 9,200 cycles | — |

(C64 figures from the Wikipedia Rugg/Feldman table and the Creative
Computing article; rough, stopwatch-era numbers.)

**Readings.**
- Rugg/Feldman scale ~40× over the C64 on the interpreter loop (RF2–7),
  which is the 40.5 MHz clock doing its job on EhBASIC's parser;
  RF8 is 400× because `^`, `LOG`, `SIN` go to the MATH unit. The
  interpreter, not arithmetic, is the cost now.
- Ahl's accuracy .0115356 is the MS-float answer (IBM PC: .01159668),
  so the MATH-unit shuffle loses nothing the 40-bit format did not
  already lose. **Gotcha:** EhBASIC's Galois RNG starts from an
  all-zero seed, so `RND(0)` returns 0 forever until one `RND(n≠0)`;
  AHL.BAS seeds on line 3 (Ahl's own note anticipates this).
- **Byte Sieve in BASIC does not fit**: `DIM FLAGS(8191)` is 48 KB of
  6-byte floats and EhBASIC at `$6C00` has 25.6 KB free. First concrete
  case for section K (or for moving EhBASIC's workspace out of the
  window); recorded, not fixed.
- **CHROUT: ~4,000 cycles per character, i.e. ~100 µs, 10 K chars/s.**
  A full 80×60 screen in under half a second. Scroll costs ~9,000
  cycles — the DMA is instant, the rest is the cc65 row-template and
  blank-row bookkeeping. So: the C ROM is not the bottleneck of
  anything a human can see; a `TYPE` of a 100 KB file is ~10 s, bound
  by CHROUT, which is the one place an assembly inner loop (or a
  "print N bytes" ROM call that walks the string itself) would pay.
  Answer to "rewrite the ROM in assembly?": no — C ROM, assembly
  leaves, and CHROUT is the first leaf if TYPE ever matters.

**Release alpha-0.1 "Proof-of-concept" (same night).** Commit 356ccae on
`master`, tag `alpha-0.1`, pushed to GitHub and the ubuntu-s1 mirror.
README.md carries the release line. The card package was rebuilt on
p15 (`~/Projects/k4510-pi/pkg/`, kernel unchanged, `rom/` and `fs/`
refreshed from HEAD, README.txt gained a BENCHMARKS section) as
`bmc-k4510-pi3-alpha-0.1.zip` (41 files, 2.3 MB) and attached to
**github.com/mlongval/bmc-k4510/releases/tag/alpha-0.1** for Doc's
friend. Note: the laptop's branch is `master`, not `main`, and
non-interactive ssh there has neither `gh` on PATH
(`/home/linuxbrew/.linuxbrew/bin`) nor a git identity — set both
explicitly when scripting.

## 2026-08-23 (small hours) — K-01..K-04 built: programs bigger than the window

Doc: "go ahead and implement K-01 through K-04". Commit `358592e` on the
laptop, pushed. `make test` = 10 suites green; `fs/segdemo.prg` is the
proof: a cc65 program whose two overlays are both linked for `$4000`,
stored at 1 MB and 1 MB + 8 KB, called through the gate with arguments
and return values passing, each RTS restoring block 2.

- **K-01 bank registers, `$D600`** (`core/mem.c`, `core/io.c`): one
  4-byte register per 8 KB block; phys = base + (cpu & $1FFF), byte
  granularity; any byte write takes effect (so `STQ` works); byte 3 with
  bit 7 set turns the block off; `$D620` = banked mask, `$D621` = MAP
  mask. Whichever wrote a block last owns it; **MAP rewrites all eight
  blocks**, so prg0's "MAP off" at exit clears the banks too.
- **K-02 far-call gate, `$DF00`**: `JSR $DF00+4n` → descriptor n of the
  table at `$DF80` (8 bytes: base, block, flags, entry) → save the
  block's bank, bank it, `pc = entry`; the callee's RTS lands on `$DFF0`,
  which restores and returns to the caller. Depth at `$DF84`, errors at
  `$DF85`, 64 deep. Flags: leave banked / don't bank (long jump). No
  core change: the core sets `old_pc` before each opcode fetch, so
  `addr == old_pc` in the read callback *is* the fetch; the gate returns
  a NOP and sets `pc = entry - 1` because the core increments after the
  fetch (the first bug of the night).
- **K-03 segment loader**: `.prg` files starting `"K4SG"`: count (≤ 8),
  flags, entry, then a 12-byte table per segment (phys, len, bank
  block), then the bytes. The ROM reads the *whole table first* (second
  bug: I read table/data interleaved), FS-READs each segment straight
  to its physical address, sets any bank. `LOAD` reports "in N
  segments"; `INFO -m` lists banked blocks.
- **K-04 `demo/far.h`**: `far_peek/poke/16/32`, `far_copy/fill` (DMA),
  `bank_set/off/get`, `BANK_WINDOW(n)`, `far_table()`, `FAR_FN(slot,
  type)` — a slot as a function pointer; A/X pass through so
  `__fastcall__` works across the gate. `demo/seg.cfg` + a 40-line
  `segdemo-header.s` build the K4SG header from ld65's
  `__OVL1_START__/LAST__` symbols.

**Lessons.** (1) cc65 `#pragma rodata-name` set to the *code* segment
emits a function's local string *at the function label*, before the
code — the symbol then points at text. Overlay rodata gets its own
segment (`OVL1R`) in the same memory area. (2) `__PRG_LAST__` includes
BSS, which ld65 does not write; the file length is `__BSS_RUN__ -
__PRG_START__`. (3) An hour lost to a stale `test/headless` linked
against the old `mem.o`: the Makefile lists `$(CORE_OBJS)` as
prerequisites, but I had rebuilt the objects by name and not the
binary. `make all` before trusting a test tool. (4) Evaluation order
of `printf` arguments is unspecified — a probe lied to me for ten
minutes.

Not done, deliberately: llvm-mos investigation (FEATURES K footnote),
the B/C ideas, and a new release zip — alpha-0.1 stays as shipped.

## 2026-08-23 (afternoon) — ROM stage 4: directories, the boxed banner, @ and CHAIN

Doc's ten-item list. Commit `2b8f657`, pushed; 10 suites green; the
emulator on the t480i5 restarted on the new ROM.

1-3. **CD, MKDIR, RM/ERASE/DEL, RMDIR** in the shell. The `$D300` device
  grew `CHDIR MKDIR RM RMDIR GETCWD` (commands 11-15), a cwd, "/" and
  ".." (never above the sandbox root), a sorted listing with directories
  as `SIZE = $FFFFFFFF`, case-insensitive lookup when the exact name is
  absent (EhBASIC upper-cases everything), and reads of a bare name fall
  back to `/PRG` then `/BASIC`. The prompt shows the cwd: `/BASIC] `.
  DIR is two columns at 80 wide, `<DIR>` entries in white. Pi: the
  shim wraps `unlink`/`mkdir` but has no `rmdir` — guarded, says "failed".
4-5. **Yellow on blue** (`C_FG = 7`), and a **boxed banner** in CP437
  box glyphs (the Linux font is CP437-ordered): name and ROM stage on
  the title row, then CPU/MEMORY, VIDEO/SOUND, FILES/BASIC, TIME/ALSO in
  two columns, one hint line. The ROM's 12 KB code half overflowed by
  1 KB; INFO/HELP/banner moved to a `CODE2` segment in the `$E000` half.
9. **`MODE a b`**: `b = 1` keeps one blank cell row on top and one
  column on the left. Implemented as physical (`PCOLS/PROWS`) vs logical
  (`COLS/ROWS`) geometry with `OX/OY` offsets; `blank_row` and `scroll`
  work on whole physical rows so the margin column stays blank for
  free. Boots in `MODE 1 1`: 640x240, 79x29. romtest's `find()` skips
  the margin now.
6. **`fs/PRG/` and `fs/BASIC/`**; `BENCH.BAS`→`FLOAT.BAS`, `BENCH2`→
  `EXPR.BAS`; Makefile, `make-sd.sh` (`cp -r`), benchmarks.sh follow.
7. **`@command` in EhBASIC** — the prefix, as recommended: keywords
  would be tokenised inside variable names and each costs a table edit;
  the prefix is two five-line patches in `basic.asm` (the cruncher copies
  the rest of the line verbatim after `@`, like REM; the statement
  dispatcher jumps to `K_AT`) and a new ROM call **`SHELL $FF8F`** (A/X =
  pointer to a command line). So `@DIR`, `@CD BASIC`, `@TYPE X`, `@INFO`,
  `@MKDIR`... all of them, present and future, from immediate or program
  mode. Gotcha: the jump-table wrapper `zp_in` uses X as its loop
  index, so `w_shell` must PHX/PLX around it — the first pointer arrived
  with a dead high byte.
8. **CHAIN = `LOAD` in a running program runs the loaded program** (C64
  semantics, no new syntax). The feed that LOAD already uses types `RUN`
  after the file; `ccflag` inhibits EhBASIC's Ctrl-C sampling during the
  feed (it was eating the `R`). Two real bugs found on the way: the
  input vector must preserve X (the line editor's index — `PLX` also
  trashed the Z flag my `BEQ` wanted), and **EhBASIC's `NEW` flushes the
  stack but returns to the top frame**, which inside `IF … THEN LOAD` is
  the IF tail, whose RTS then jumps to `$0001` and the CPU walks up RAM
  into EhBASIC's entry — a "cold start" that took a PC trace to explain.
  LOAD now does `JSR LAB_1463 ; JMP LAB_127D` (the immediate loop).
  Variables do not survive a chain; `PEEK(1039)` is the "came from a
  menu" flag the menus POKE.
10. **`DEMOS.BAS` and `BENCH.BAS`** menus; every demo and benchmark ends
  with `IF PEEK(1039) THEN LOAD "<menu>.BAS"`; benchmarks wait for a key
  first. `test/headless` gained `~` = wait 30 frames in the key string,
  and a `K4510_DUMP=addr,len` memory dump; it now types slowly enough for
  menus.

Not done: a new card zip (alpha-0.1 stands); the `rmdir` shim on the Pi.

## 2026-08-23 (later) — menu return, DUMP, SID demos, a Ctrl-C that keeps its hands off

Commit `1a73d89`, pushed; 10 suites green; emulator restarted on the laptop.

- **"LINES does not return to the menu"** — it did; the prompt was
  invisible. `PALETTE 1..15` recoloured the ROM's own text colours (6
  blue, 7 yellow) and `GRAPHICS 2` left the chip in 640x480 under a
  240-line text layout. Fix at the root: **`VIDEO $FF92`**, a ROM call
  that re-applies the text mode and palette; `GRAPHICS 0` calls it. The
  demos use colours 16+ now. (First attempt saved the palette in
  `$0410-$043F` — which is the expression compiler's state; the
  resulting "copy: from.to dest" from nowhere cost an hour. The ROM owns
  its palette; EhBASIC asks.)
- **`DUMP [note]` / `@DUMP note`** (item 3): SYS `$D5F0` write → the
  host writes `dumps/dump-NNN.txt` with CPU, MAP/banks/far gate, VICKe
  layers, SID shadows, FS/DMA/MATH registers, the text screen, the
  shell log (every shell line + DUMP notes, via `$D5F1`), the last 256
  keys, the last 4096 opcode fetches (runs collapsed), zero page,
  stack, `$0300-$04FF`. Read it over ssh:
  `t480i5:~/Projects/BMC64k4502/k4510/dumps/`.
- **Three SID demos in BASIC** (item 4): `SIDWAVE` (one voice, four
  waveforms, a scale each), `SIDBEAT` (bass on SID0, noise drums on
  SID1), `SIDFILT` (a sawtooth chord through a swept low-pass with
  resonance). DEMOS menu entries 6-8. Subroutines at 9000+ because a
  900-line subroutine sits *between* 160 and 990 in line order and gets
  fallen into — an old BASIC lesson relearned.
- **Keys typed while a program is busy were lost.** EhBASIC's Ctrl-C
  check pops the input every statement and keeps the byte 32 statements;
  a frame-wait loop burns that in a millisecond. Now the keyboard
  device has a **peek register `$D102`** and `k4510_cc` only looks,
  popping nothing but `$03`/ESC. Needed the `PG2_TABS` table entry in
  `basic.asm` patched, not a store before `LAB_COLD` (which re-copies
  the table). Side effect: RF2 14 → 10 frames — the old check went
  through the ROM's GETIN every statement.
- **Item 2 (ROM vs RAM)** written up as **K-05** in FEATURES.txt with a
  DEFER suggestion: the C64 ROM-out trick maps onto the bank registers
  in a day, but only the BASIC Sieve has ever hit the 38 KB, and for
  BASIC the lever is EhBASIC's own workspace.
- **Item 5 (Pi speed switch): not needed.** The desktop frontend is
  vsync-locked at 60 Hz with the same 40.5 MHz cycle budget per frame;
  the Pi measured 40.42 MHz and 60 fps ("the desktop number exactly",
  2026-08-23 morning). What you see on the laptop *is* the Pi.

## 2026-08-23 (evening) — K-05 built: RAM under the ROM; DUMP ON

Doc: "I can already hear the critics saying the ROMs eat all the RAM …
if it's not expensive, implement it now." Commit `faf0196`, pushed,
10 suites green, emulator restarted.

**As built.** The ROM image no longer sits in the low 64 KB physically:
`mem_load_rom` puts it at `$0FFF0000 + addr` (top of the 256 MB) and the
unmapped CPU view reads it from there above `mem_rom_base`. So physical
`$A000-$FFFF` is plain RAM *under* the ROM. `rom_out()` (far.h) banks
block 5 onto `$A000` and block 7 onto `$E000` — identity banks — and the
program has `$0800-$BFFF` + `$E000-$FEFF` = **54 KB**. The page
`$FF00-$FFFF` is hard-wired to the ROM whatever is banked (the 6510's
vectors, generalised): it holds the jump table, the vectors and the
**stub** — every system call goes `JSR $FF80` → `rom_push` (saves bank
registers 5-7, 12 bytes, on the stack; banks the ROM in) → the wrapper
→ `rom_pop` (restores, byte 3 of each register last). The IRQ entry
does the same inline with no temporaries, so it may land anywhere,
including inside `rom_push`. A reset clears all banks first (F12 does
not reset the MMU). The ROM keeps `$C000-$CFFF` (it does not fit in two
blocks) and I/O keeps `$D000`. `romout.prg` proves it: fills both
ranges with the ROM out, prints through CHROUT meanwhile, 0 bad bytes,
pattern intact under the ROM afterwards. `INFO -m` says so.

**Bank-register semantic changed** to make byte-wise save/restore safe:
bytes 0-2 set the base, **byte 3 switches** (bit 7 = off). Before,
writing byte 0 of an "off" register switched it on with a partial base —
fatal when the register is block 6's (the I/O page vanishes under a
transient bank and the next STA never reaches the registers).

**Cost, measured:** CHROUT 4,083 → 4,471 cycles (+9%) — the 12-byte
save/restore is ~300 cycles, not the 40 I promised; compute unchanged
(sieve 0.93 s). RODATA moved to the `$A000` half; the stub page took
256 bytes of the `$E000` half.

**Three stub bugs, all mine, all found by `romout.prg` and the CHROUT
benchmark:** (1) a single fixed save slot — an IRQ inside a call saved
"off" over the program's "on": save on the stack instead; (2) the IRQ
path sharing the call path's temporaries — garbage A/X when the vblank
hit inside `rom_push`: the IRQ path is inline and temp-free now;
(3) restoring byte 0 first switched blocks on — hence the semantic
change above.

**`DUMP ON` / `DUMP OFF`** (`@DUMP ON` from BASIC): SYS `$D5F2`; the
host writes a dump every 900 frames (15 s of machine time) while on.

## 2026-08-23 (evening, 2) — what the dumps said

Doc ran the machine for two minutes with `DUMP ON` and reported: SID
demos 1 and 2 silent, STARS unstoppable. Twenty-two dumps. Commit
`da60fdc`, pushed, 10 suites green.

- **Silent SID demos — dump 019 had it.** `SID0` register 23 was still
  `$F7` from SIDFILT: voices 1-3 routed into the filter, and SIDWAVE /
  SIDBEAT never touch the routing, so with `$D418` mode bits clear the
  voices went into a filter with no output — silent on a real SID too.
  Headless both demos rendered fine on a *fresh* chip, which is why I
  had not seen it. Every demo now clears all 25 registers first and
  SIDFILT leaves the routing clear. Verified by recording the SDL
  frontend's actual output (`SDL_AUDIODRIVER=disk` + the new
  `K4510_KEYS` env var that types into the frontend): SIDFILT then
  SIDWAVE, RMS 755-1887 where it used to be 0.
- **STARS unstoppable — dump 022's PC history had it.** The Ctrl-C
  check ran and saw a key, but the key at the *head* of the FIFO was the
  `j` Doc typed first; the ESCs and Ctrl-Cs were queued behind it, and
  my "peek only" check looks at the head. Now the keyboard device has
  **`$D103` break-pending**: an ESC or `$03` anywhere in the queue is
  removed and returned; other keys stay for `GET`. Tested with junk
  typed ahead of both.
- **RUN/STOP on the desktop = Esc** (Shift+Esc quits the emulator, F12
  resets); Ctrl-C = STOP. Now in the README.
- **Banner:** a 16-colour bar under the title, its inverse (upper
  half-blocks) beneath, and every row inside the box keeps its
  verticals, blank ones included. First cut wrapped by one cell: the
  content width between the verticals is `COLS-3`.

## 2026-08-23 (evening, 3) — the logo, and MON

Doc's screenshot (`k4510/screenshots/Pasted image.png`): the box's
vertical glyph has gaps in the line-doubled 640x240 mode, so blank
rows looked broken — and he wanted the MEGA65 idea, not a box: a colour
triangle with the text beside it, and nothing but name, speed, RAM.
Commit (laptop) after `da60fdc`; 10 suites green; emulator restarted.

- **Logo:** five rows of colour blocks, widths 12/8/4/8/12 (an
  hourglass, left-aligned), red/orange/yellow/green/light blue; to the
  right: `BMC-K4510`, "a fantasy 8/16-bit computer", `45GS02 at 40.5
  MHz`, `256 MB`. No box. `INFO` has everything else.
- **`MON`:** the Wozmon grammar left the shell's fallback and became a
  command. `MON` opens the monitor at a `*` prompt (`addr`, `addr.addr`,
  `addr:b b`, `addrR`, shell commands too), `X`/`EXIT`/`Q` leaves;
  `MON E000.E00F` runs one line without the prompt. `@MON` from EhBASIC
  returns to BASIC on `X` (it is a nested readline inside the SHELL
  call). Unknown shell words now get "? (HELP lists the commands; MON is
  the monitor)". romtest's monitor lines carry `mon`. The ROM's BSS is
  full to the byte (the monitor reuses the shell's line buffer).

## 2026-08-23 (night) — six and twelve voices; the scrolling benchmarks

Commit `8178e76`, pushed, 10 suites green, emulator restarted.

- **AHL/FLOAT/EXPR "scroll off screen"**: their wait line was
  `900 PRINT: PRINT "ANY KEY...";: GET K$: IF K$="" THEN 900` — the
  PRINT re-ran on every poll and scrolled the results away. Split into
  900/905 in all twelve benchmark files (the same mistake I had already
  fixed once in SIDWAVE). README.BAS got a key wait; STARS leaves on any
  key (`PEEK(53505)` inside its loop); `RUN ehbasic` tries `ehbasic.prg`
  (the dumps showed Doc typing it).
- **`demo/sidorch.h`**: a SID orchestra — Pachelbel's progression in D,
  8 bars at 120 bpm, an eighth per 15 frames. `sid6.prg` (2 chips: bass
  through the low-pass, chord 3rd/5th, melody, 16th arpeggio, drums) and
  `sid12.prg` (4 chips: plus a slow triangle pad on the chord, the melody
  echoed two eighths late, a second bass an octave up on the off-beats,
  hi-hats) — one source, `NCHIPS`. A row per voice on screen with the
  note and a level bar. cc65 did not explode: 5.2 / 5.7 KB.
- **`SID6.BAS` / `SID12.BAS`**: the same music in EhBASIC, one note per
  eighth (no 16ths), generated from one template. Fast enough: the
  whole 12-voice step is ~40 POKEs in a 7-frame half-eighth. Gotcha:
  EhBASIC's `AND` is 16-bit signed, so `Q AND 255` on a frequency word
  above 32767 (the hi-hat) is a Function call error; use
  `Q-INT(Q/256)*256`. DEMOS menu: 9 and 0.
- **Sieve.bas out of memory — why:** `DIM FLAGS(8191)` is 8192
  six-byte floats = 48 KB; EhBASIC has 25.6 KB between its program area
  ($0800) and its own code ($6C00). The 256 MB are behind the CPU's
  64 KB view and EhBASIC (1980s design) keeps arrays in the view. The
  honest fixes: integer arrays (two bytes each — EhBASIC has none, a real
  addition), or moving EhBASIC's code above the window with the ROM out
  (K-05 makes $A000-$BFFF + $E000-$FEFF available, but EhBASIC is 12.8 KB
  in one piece and neither range holds it). Left as a ballot item.

## 2026-08-23 (night, 2) — voice toggles, DUMP ON by default, Escape is STOP

Commits `a9f0c3c`, `75de6c8`; pushed; 10 suites green; emulator restarted.

- **Auto dump on at reset** (Doc: "until further notice"); 100 files
  rotating in `dumps/`. `DUMP OFF` turns it off.
- **Voice toggles** in `sid6`/`sid12` (keys `1-9 0 A B`, the key stands
  in front of each row, a muted row shows "off"; `Q`/Escape leave) and in
  `SID6.BAS`/`SID12.BAS` (same keys, the list redraws; the play subroutine
  skips a muted voice via `U()` — `M()` was already the minor flags,
  "Double dimension Error").
- **Escape in BASIC = STOP** (Break to Ready, and `k4510_hush` zeroes
  all four SIDs' registers so nothing hangs), the same as Ctrl-C. It
  used to jump through the reset vector — Doc's "hangs and drops to the
  OS". **`@BYE`/`@EXIT`/`@QUIT`** leave BASIC for the shell deliberately
  (handled in `K_AT` before the text reaches the ROM shell).

## 2026-08-24 — SIDPLAY: real C64 music on the fantasy machine

Doc: "write a SID player, there is a whole folder of sid tunes" — 199
PSID/RSID files in `k4510/sidfiles/` (EC64SC collection, tracked).
Commit `d43453e`, pushed, 10 suites green, emulator restarted.
`fs/SID` is a symlink to the collection.

**`RUN SIDPLAY`.** A chooser first (Doc asked mid-build: no typing) —
two columns, cursor keys/PgUp/PgDn/Home/End, Enter plays, Esc leaves.
Playing: title/author/released from the header, load/init/play
addresses, song n of m (+/- switches), a clock, and a live meter per
voice (waveform name + a bar from the frequency register). Space jumps
to the next tune. RSID files (15) and play-address-0 tunes need a real
C64 (KERNAL, CIA) and say so honestly.

**How it runs a real C64 tune.** The tune's own 6502 code is the
player: copied to its C64 load address and called at init, then play at
50 Hz (PAL) or 60 (NTSC/CIA) from the frame counter (5 calls per 6
frames). For that the tune must own the C64's memory, so the player
itself lives *under the ROM* at `$E000-$FEFF` (sidplay.cfg + a K4SG
entry stub at `$0250` that banks block 7 and jumps — the loader must
not bank it: the ROM still runs from block 7 until the program is
entered; found the hard way, SP=$F398). The player swaps only its own
36 zero-page bytes ($40-$63) around each call into the tune — the tune
keeps the rest of the zero page. VICKe registers the tune pokes
(thinking they are the VIC-II) are put back every frame.

**The machine grew one rule for it:** the I/O page `$D000` and the stub
page `$FF00` are now *always* what they are, whatever MAP or the banks
say (before, I/O existed only in the unmapped view). So block 6 banks
to RAM like 5 and 7 — 57 KB with the ROM out, and the 19 tunes that
load into `$C000-$CFFF` (Armalyte, Last Ninja 2, R-Type...) work;
verified Armalyte playing from under the ROM. Commando (Rob Hubbard,
11 songs) is the smoke test: RMS 1500-2000 from the recorded SDL path.

**Exit** is its own trick: the player cannot un-bank block 7 from code
inside block 7, so `_exit` copies a MAP-off trampoline to `$0230` (the
first version used `$0200` — the ROM's DATA segment, oops) and jumps.
The caller's directory is restored (the player CDs to /SID).

## 2026-08-24 (b) — Cobra's revenge: load-range checks and the PAL crystal

Doc: "I played the first one, fine; then Cobra.sid and the emulator
exploded." Cobra loads at **$F000-$FFF0 — straight over the player**
at $E000-$FEFF; the far_copy overwrote the running code. Now the
player checks the range first: tunes outside $0300-$CFFF show their
header and "the player itself lives up there" instead of loading —
30 of 184 PSID files (the under-the-KERNAL crowd: ACE II, Spellbound,
Miami Vice...). Supporting them would need a player that does not
live at $E000; parked.

He also asked whether 50 Hz tunes are speed-adjusted: the *call rate*
was right (50/60 via an accumulator), but the *pitch* was 1.5% sharp —
our SIDs ran at exactly 1 MHz, a PAL C64's at 985248 Hz. New SYS
register **$D5F3**: SID crystal 0/1/2 = 1 MHz / PAL / NTSC (reSID
set_sampling_parameters per chip); the player sets it from the tune's
flags and puts 1 MHz back on exit. The play screen says "50 Hz PAL".
Commit `<this>`, pushed; 10 suites green; emulator restarted.

## 2026-08-24 (c) — the 45GS10, bare-name RUN, a kinder chooser, the Sieve solved

Doc's nine-item list. Commits `0e376d2` + the AHL seed fix; pushed;
10 suites green; emulator restarted.

1. **The CPU is the 45GS10 now** — Doc: it grew the 6510's ROM/RAM
  trick (and the rest of the K4510 MMU), so it earned its own number,
  and it lines up with the machine name. All user-visible strings and
  docs; the Xemu core file keeps its own name (it is byte-for-byte the
  45GS02 instruction core). Machine name stays BMC-K4510.
2. **A bare word at the prompt runs a program**: unknown commands try
  `name` then `name.prg` through the search path, so `SIDPLAY` works.
  (The OS has no name, by the way — the log calls it "the system ROM".
  Candidates if Doc wants one on the ballot: naming it is his call.)
3. **Prompt history** (answered): `]` is the Apple II lineage (Applesoft
  prompt; Integer BASIC used `>`), `>` is the CP/M-to-DOS lineage, C64
  BASIC had none ("READY."), Wozmon used `\\`. With a cwd in the prompt,
  `>` reads DOS-ish, `]` reads Apple-ish; ours is `]` after Wozmon
  heritage. Doc's call whether to switch.
4/7. **The chooser redraws only the two affected rows** on a cursor
  move (full redraw only on a page change), and **drains the whole key
  queue before drawing** — holding a key no longer floods the FIFO
  faster than redraws drain it.
5/6. **Plans, not code yet** (answered in the reply): play-address-0 /
  CIA tunes need a small CIA timer device + a RAM IRQ vector honored by
  the player; RSID needs a real KERNAL — both parked. The $E000-crowd
  fix is designed: keep the tune's image in far memory and have a low-RAM
  runner bank block 7 to the tune around each play call and back to the
  player after; tunes with data in $FF00-$FFF9 stay impossible (the
  stub page always reads ROM). ~22 of the 30 rejects would come back.
8. **SIEVE2.BAS**: the full 8191-flag Byte Sieve in BASIC after all —
  bank register 1 maps 8 KB of far memory into $2000-$3FFF and each
  flag is a POKEd byte instead of a 6-byte float. 4.79 s for one
  iteration (C: 0.093 s/iteration). SIEVE.BAS stays in the menu as
  "the lesson". This is K-01 doing real work from BASIC.
9. **Every benchmark explains itself** (REM header: what the loop
  measures) **and prints era comparisons** — C64/Apple IIe (1 MHz MS
  BASIC), BBC Micro B, MEGA65 estimate — marked stopwatch-era and
  approximate; Ahl's uses the 1984 article's own table. Merging the
  extra lines by line number replaced AHL's RND seed line (line 3) —
  caught by the runner (RANDOM 1000), renumbered.

## 2026-08-24 (d) — the card staged, the guide begun, and Space Invaders

(Recovered entry: this and the next were mistakenly committed into the
k4510-build clone; rewritten here where they belong.)
Commits `669b6b2`, `741035f` (guide), `8c19077`. The Pi kernel rebuilt
on p15 (the circle env is pinned to the 15.2 ARM toolchain; 14.2 gives
header soup); `pkg/` = the full card. SID stats: 155/199 play (77%).
EhBASIC `RUN "name"`; the OOM sieve retired; the User's & Programmer's
Guide begun in `doc/guide/` (A5 XeLaTeX, Neo6502-style, screenshots
captured from the machine at build time, builds on ubuntu-s1 in the
`k4510-build` clone, 17 pages, PDF sent). Benchmarks re-reported
(clean bodies, THIS MACHINE line, aligned era tables, metric stated;
S/E marks dropped; AHL/FLOAT/EXPR full reports). `INVADERS.BAS`:
Space Invaders in EhBASIC on the banked text screen — the screen is
9,600 bytes, TWO bank registers; one window left the player's row
poked into invisible plain RAM. EhBASIC has no ERASE (wave 2 would
have double-DIMmed); landing invaders end the game. DEMOS menu key I.

**Card written, 2026-08-24.** 3.7 GB FAT32 "K4510", firmware + new
kernel + rom + fs + 199 tunes, kernel and ROM verified byte-for-byte.
**But**: staged BEFORE the invaders/benchmark commits — INVADERS.BAS
is not on it (Doc noticed immediately). Re-staged in the next entry.

## 2026-08-24 (e) — the Pi's choppy SIDs, and the card rewritten

Doc from the TV: INVADERS.BAS missing (the card had been staged before
that commit — mine), and "the sid sounds on the RPI3 are very very
bad, choppy choppy choppy". The laptop was suspended, so the fix was
made in the ubuntu-s1 clone (`k4510-build`), pushed to the mirror
(commit `09919dd`); the laptop pulls when it wakes.

**The chop:** a performance regression, not an audio bug. The Pi 3 ran
the machine at ~17 ms against a 16.7 ms frame — no slack — and two
recent changes spent per-access time: the debug recorder stored every
opcode fetch address (a store per emulated instruction), and the
"I/O page always wins" rule put an extra compare first on *every*
memory read and write. Invisible on the laptop, over budget on the Pi:
the audio ring starves and stutters. Now the fast path is back to its
one-compare shape (the I/O check lives inside the unmapped/banked
branches, same semantics, all tests green) and the PC recorder is
armed only when dumping is on (`DUMP` / `DUMP ON`; armed at reset on
the desktop, off on the Pi).

**The card**, rewritten and verified: fixed kernel, current fs (26
BASIC files including INVADERS.BAS, 199 tunes), README. Along the way:
two log entries had been committed into the k4510-build clone instead
of this repo (wrong cwd) — clone reset, entries recovered above.

## 2026-08-24 (f) — no more Memory size ?, LOADED, and sprites in BASIC

Doc's three. Commits `162aaa6`, `711f6b5`; pushed; 10 suites green;
emulator restarted.

- **"Memory size ?" is gone** — the cold start falls straight into the
  auto-size probe (the machine knows its memory).
- **A plain `LOAD` prints `LOADED`** (chains stay silent). The message
  is printed from inside the input-vector feed with the line editor's
  X and Y preserved — the first attempt returned with X = message
  length and the editor committed stale buffer text ("OS.BAS"" →
  Syntax Error). A second attempt wiped the feed's byte-reading code
  by slicing at the wrong `k_fi_end` (the BEQ reference, not the
  label): LOAD printed LOADED and loaded nothing. Both found by the
  headless harness; the file was reset to git HEAD and patched once,
  cleanly. Lesson relearned: patch-stacking an asm file is how you
  make it lie to you.
- **EhBASIC has sprites**: `SPRITE n,x,y` (position + enable),
  `SPROFF n`, `SPRDEF n,page,w,h,bpp` — the data address is given in
  256-byte pages so a 16-bit argument reaches 16 MB; w/h from
  8/16/32/64; 4 bpp uses palette 16-31 (palofs 1). Three new tokens
  wired through all four EhBASIC tables (defs, CTBL vectors, crunch
  dictionary, LIST names); the attribute table lives at $03F800,
  cleared by DMA on first use, and every sprite statement re-points
  VICKe at it — so the ROM's video restore (sprites off) is undone by
  the next statement.
- **`INVADER2.BAS`** (DEMOS key J): Space Invaders with hardware
  sprites — 24 invaders + ship + bullet + bomb as 16x16 4-bpp shapes
  poked into $040000 through a bank window from DATA rows, pixel
  coordinates, smooth ship. One bug: shapes were poked 128 bytes apart
  but SPRDEF pages are 256 — the player wore the bullet's costume and
  the bullet pointed at zeroes.

## 2026-08-24 (g) — INVADER2 plays like the real thing

Doc: one bomber, too fast, slow ship, no bases. The first three were
ONE bug: the game never seeded EhBASIC's RNG, and an unseeded RND(0)
returns 0 forever — so the "random" column was always column 0 and
RND(0)<0.03 was always true (constant fire). Seeded; the firing rate
is 0.02+wave*0.005 per frame; and the classic rule: the LOWEST living
invader of a random column drops the bomb. The ship reads every queued
key per frame (step 8) instead of one. And four bases, 32x16 sprites
with an arch, each owning its own shape page: a hit (bomb from above,
bullet from below) pokes a hole into the shape through the bank
window — real erosion, visible in the screenshot. Commit on the
laptop, pushed; emulator restarted.

## 2026-08-24 (h) — the teleporting bullet, and the real invaders

Doc: the shot leaves the ship, then "comes out of the rightmost base".
The frame-by-frame sprite trace showed the bullet's X becoming 500 the
moment it entered the base band — and the cause is a museum piece:
**EhBASIC, like every MS-family BASIC, reads only the first two
characters of a variable name.** The base test's loop variable `BX0`
IS the bullet's `BX`; each pass left BX at the last base's X (500).
Renamed to QX, with a REM in the file so the next 8-year-old learns it
the easy way. (Two lesser bugs en route: a REM appended mid-line
swallowed the rest of the statement, and a rebuilt line 205 blew the
126-character line limit — "Syntax Error in line 205" from the feed.)

The sprites are the arcade originals now, encoded from the canonical
patterns (no need to scrape them: squid 8px, crab 11px, octopus 12px,
the stepped cannon, the zigzag bomb) — squid row white on top, crabs
yellow, octopuses green, 30/20/10 points by row as in 1978. Six shape
pages; the bases moved to pages 1030-1033 and the erosion offsets
followed. Commit `<laptop>`, pushed; emulator restarted.

## 2026-08-24 (i) — alpha-0.2 shipped, the card written, and the Tube

Doc: write the card, cut the release, then "port BBCSDL — and BBC BASIC
must see the 256 MB, not the 64K business; the ROMs are too big."

**alpha-0.2 "Octopodes"** is on GitHub with the full card zip; the SD
card itself written and verified on p15 (after one polkit stand-off:
ssh cannot mount removable media there — the desktop session's
auto-mount does it, so the card must be freshly inserted).

**BBC BASIC.** BBCSDL's interpreter is compiled C for x86/ARM — it
cannot run ON a 6502-descendant, and no memory trick changes that. But
Acorn answered this exact question in 1981: the **Tube**. BASIC runs on
a co-processor with its own flat memory; the host machine does the I/O.
So the K4510 grew a Tube at `$D800`: the emulator host runs Richard
Russell's BBCTTY console edition (vendored under `tube/`, zlib licence,
alterations marked; the name "BBC BASIC" is the BBC's, used by
Russell's permission — the docs say "running Russell's interpreter"
and claim nothing) on a pty; the ROM's `BBC` command is the console.
Three terminal puzzles on the way: the console edition asks
`ESC[6n` ("where is the cursor?") and hangs without an answer; a wrong
answer is worse — it probes the terminal WIDTH by jumping to column
999 and asking where it landed, so the guest must honour `ESC[r;cH`
with clamping and answer with its true cursor; and my first host-side
answerer replied "column 1" to everything, which made BBC BASIC
believe the terminal was one character wide and print the banner
vertically, one letter per line.

**The requirement, met:** `HIMEM=PAGE+250*1024*1024` → "250 MB FOR
BASIC"; `DIM A 200000000` succeeds; byte 199,999,999 pokes and reads
back. MAXIMUM_RAM patched (marked) from 4 GB to 256 MB to match the
machine. The Pi has no Tube yet — the C core would compile into the
Circle kernel (bbdata_arm_64.s exists) as phase 2, on the ballot.

## 2026-08-24 (j) — star commands, and the OS has a name: K/OS

BBC BASIC's `*` prefix now does what it did on a real BBC — talks to
the operating system. BBCTTY's OSCLI routed every unknown star command
into the host's `system()` (a host leak into the machine); the marked
patch emits `ESC ] K4510 ; <cmd> BEL` instead, and the console executes
it in the machine's shell. So `*INFO`, `*ECHO`, `*MODE`, even `*BALLS`
work from inside BBC BASIC; BBC keeps its own stars (`*QUIT`, `*FX`,
`*LOAD`...), and the co-processor is homed in `fs/`, so its own file
commands see the machine's filesystem. (`@` stays EhBASIC's escape —
in BBC BASIC `@` is the `@%` print-format variable and must not be
taken.) The ROM's BSS was 17 bytes short for an OSC buffer; the shell
line buffer serves.

**And the OS is named: K/OS** — pronounced "chaos" — Doc's pick from
the shortlist (FRED is reserved for the MATH unit's future, JIM for
whatever earns it; SHEILA already works here). Banner, INFO, README.
Commits `641d9ec` + one; pushed; emulator restarted.

## 2026-08-24 (k) — one star to rule them

Doc: "@ for EhBASIC and * for BBC BASIC — too confusing?" Agreed.
Unified on `*`, the Acorn-correct choice: `*DIR` now runs a K/OS
command from EhBASIC too. It could not be done at the crunch level the
way `@` was — `*` is multiplication everywhere except the start of a
line — so the cruncher remembers where the statement text starts
(`k_crx0`) and only a line-leading `*` is a prefix; `PRINT 2*SIN(1)`
is untouched. The reverse unification was impossible: in BBC BASIC `@`
is the `@%` print-format variable. `@` remains a quiet EhBASIC alias.
Menus and READMEs teach `*`. Commit pushed; emulator restarted.

## 2026-08-24 (l) — the full BBC BASIC: graphics and sound over the Tube

Doc asked for the full BBCBasic/SDL experience on the machine (with the
RPi3 port to follow). The SDL edition would have opened its own window
on the host — illusion gone — so instead the *console* edition grew the
missing chips, the Acorn way: it already serialises every graphics
command as VDU bytes (`PLOT` **is** `VDU 25,k,x;y;`), and SOUND was an
empty stub. `bbccos.c` now forwards the graphics codes as
`ESC]K4G;...BEL`, `sound()` emits `ESC]K4S;...BEL`.

First attempt interpreted them in the console ROM: **2 KB over the
24 KB ROM budget**. The fix is nicer than the plan: the **Tube ULA**
(core/io.c) — on a real Beeb just the FIFO chip — watches the byte
stream itself and executes the machine escapes before the console ever
sees them. K4G drives the VICKe blitter: 640x480 8bpp bitmap at
$200000 (EhBASIC's GRAPHICS surface), layer 1 over the text, colour 0
transparent, BBC 1280x1024 bottom-left coordinates scaled on, BBC
logical colours in palette 16-31 with per-MODE default maps. PLOT does
lines, points, triangle fill, rectangle fill, circles (outline+fill,
scanline + isqrt); GCOL, CLG, VDU19, VDU29 origin all work. K4S feeds
a new **sound sequencer at $D5E0-$D5E3**: the Beeb's four queued sound
channels in K4510 silicon — channels 1-3 pulse on SID 0, channel 0
noise on SID 1, quarter-semitone pitches, frame-clocked durations,
**64 notes deep per channel** (the Beeb blocked BASIC at four pending;
a one-way Tube can't block, so the queue got deep enough for a whole
tune instead). The ROM keeps only what it must: MODE switches the text
geometry (the ULA passes 22 through), SGR colours (COLOUR works now),
ESC[J (CLS clears), restore on exit — ~400 bytes.

`fs/BBC/`: seven period-style demos — KALEID, CIRCLES, ROSES, CLOCK
(runs on TIME$, i.e. the host clock), TUNE (Frère Jacques as a
three-voice round — the sequencer's party piece), BOUNCE, MOUNTAIN —
plus README.BBC. BBC BASIC LOADs the ASCII listings directly.
Verified headless: a MOVE/DRAW put its pixel exactly where the scaling
predicted (row 240, x 333, colour 19), triangle fill solid, the round
showed three voices live on SID 0. All 11 suites green. Commit
`73b6519`, pushed; emulator restarted. Pi note: the ULA + sequencer
live in core/io.c, which the Pi kernel compiles — when the co-processor
moves into the Circle kernel (phase 2), graphics and sound are already
there. Known edges, on the record: POINT/TINT read-back not
implemented; GCOL modes 1-4 (OR/AND/XOR) drawn as plain; VDU5
text-at-graphics-cursor prints as normal text; `*DUMP` inside BBC
BASIC is taken by BBC's own *DUMP (use it from the shell).

## 2026-08-24 (m) — the filesystem gets its names

Doc: `/BASIC` → `/EHBASIC`, `/BBC` → `/BBCBASIC`. Renamed (git mv), and
the bare-name search in the fs device now looks in `/PRG`, `/EHBASIC`
**and** `/BBCBASIC`; Makefile, shell HELP, README.md, README.BBC and
the DEMOS menu all follow. `RUN EHBASIC` still works (found by search),
`LOAD "BBCBASIC/KALEID.BBC"` is the new BBC path. Commit `eedde0f`,
pushed both remotes; emulator restarted.

## 2026-08-24 (n) — demos live with their language

Doc ratified the layout: no separate demos folder — each language's
demos live in its own directory. The tree already complied (`/EHBASIC`
holds the EhBASIC demos + interpreter, `/BBCBASIC` the Tube demos,
`/PRG` the machine-code ones, `/SID` the tunes); the convention is now
written into README.md so it stays that way. Commit `38ea68a`.

## 2026-08-24 (o) — K/OS grows up: commands, ARGS, !BOOT, hidden files

Doc's three asks became one release (`e918f8b`): new shell commands
**RENAME/REN/MV, CP, XD (alias HEX — classic hex+ASCII dump, Esc
stops), EXEC, HUSH, LS, WOZ** (Doc remembers the monitor as WOZ; now
it answers to it), **DIR A** for dot-hidden files (ratifying what the
fs device already did), **/!BOOT** as a power-on EXEC script
(half-second grace; any key — held or typed — skips silently without
eating the keystroke), and the **REXX rule completed**: an unknown
shell word runs `name.prg` from disk *with arguments*, readable via
the new **ARGS system call ($FF95** — the last free jump-table slot;
stub page and jump table are now full to the byte). `SAY HELLO` →
`fs/PRG/say.prg` prints HELLO. io.c gained fs commands 16–18
(RENAME, COPYFILE, DIR_ALL).

The evening's great debugging comedy: every new .prg looked crashed —
screens wiped, no output, hours of bisecting ROMs that were entirely
innocent. The culprit was `run_at()`'s **unconditional `cls()` when a
program returns** (why chrout.prg ends with "press a key"). SAY would
print and the ROM would erase it a millisecond later. Fix: snapshot
the VICKe controls around the call; only a program that actually drew
gets the video restore + clear. Print-only programs now behave like
commands — output stays.

ROM pressure: both halves ran out twice. HELP's text moved out of the
ROM into the dot-hidden **/.HELP** file (HELP = TYPE /.HELP), the new
file commands rebalanced into the $A000 half, and four initialized
statics went to BSS — under 100 bytes spare in each half now. The
next feature pays rent or lives host-side. test/headless gained
K4510_EXITDUMP=1 (dump on exit — found the cls bug); romtest boots 40
frames to cover the grace window. 10 suites green; emulator restarted.

## 2026-08-24 (p) — a third language: FORTH (Tali Forth 2, native)

Doc: "how about a simple Forth interpreter?" Chose **Tali Forth 2**
(Scot W. Stevenson / Sam Colwell — public domain, ANS-core, written
for the 65c02 with a three-routine porting surface) over the 1980
FIG-Forth listing. Vendored unmodified in `forth/tali/`;
`forth/platform.asm` is the *entire* port (~100 lines): CHROUT/CHRIN
through the ROM jump table (wrapped to preserve X/Y), `KEY?` peeking
$D101 without consuming, memory map, and the .prg header. Assembles
with the already-installed 64tass; no Rockwell opcodes anywhere, so
the 45GS10 runs it as a strict 65c02 (with `LDZ #0` pinned at entry).
Kept the interactive assembler and DISASM — on this machine they are
the point; dropped blocks and ed (no block device yet). 17 KB image
at $4000, dictionary RAM $0900–$3BFF (~12.8 KB), typed `FORTH` at the
shell thanks to the REXX rule — `/FORTH` joined the host-side
bare-name search for zero ROM bytes. The ROM did not grow at all.

Two real bugs, both instructive. First: typing `forth` printed `?` —
the fs device happily `fopen()`ed the `/FORTH` *directory* (Linux
allows it), the header read failed with "bad file", and the shell
only retries `.prg` after "not found". A directory is now not-found
when opened as a file. Second: BYE hung the machine — Tali's COLD
resets the stack pointer to $FF and claims the whole stack page, and
its default RAM map ($0200 buffer, $0300 dictionary) sat directly on
the ROM's data/bss/C-stack at $0200–$07FF. Forth's RAM moved to
$0800–$3FFF, and kernel_init now saves the stack bytes above the
entry SP (K/OS's call frames) so BYE can put them back: `bye` returns
to the shell with the screen, the shell log, and the session exactly
as they were. Verified: `2 3 + . 5 ok`, `: cube dup dup * * ; 7 cube
. 343`, DISASM listing our own kernel (flagging the LDZ byte with a
polite `?`), the assembler assembling, then `bye` → `dir` → `type`
all live. `/FORTH/README.TXT` on the disk is the crib sheet. 10
suites green; commit `bf0cfc3`, pushed both remotes. Parked: a
file-loading word (`INCLUDED` through the fs device at $D300) so
Forth source can live on disk like everything else.

## 2026-08-24 (q) — the Z80 second processor: CP/M on the Tube

Doc, after Forth: "can you make a tube out of this?" — *this* being
RunCPM (github.com/MockbaTheBorg/RunCPM). Which is historically
delicious: Acorn's Z80 Second Processor of 1984 existed to run CP/M
over the Tube, so plugging RunCPM into our Tube slot rebuilds a real
product. Vendored unmodified (MIT) in `cpm/src/`, built with
`CCP_INTERNAL` so **no DRI binaries ship** — the licence table stays
clean. The Tube start register grew a program code (1 = BBC BASIC,
3 = CP/M); the ROM's `CPM` command is just `cmd_bbcbasic(3)`; drives
A:–P: are `fs/CPM/A`..`P` with user areas as numbered subfolders, so
CP/M files are ordinary machine files. RunCPM's banner proudly
estimates the Z80 at ~3000 MHz — the fastest Z80 Acorn never sold.

The ROM extracted its rent for the ~30-byte `CPM` command: DATA
overflowed ROM2 by 33 bytes, so `cmd_hush` emigrated to the $A000
half and the no-Tube error lost its subclauses. (First relocation
attempt landed cmd_hush inside run_at's CODE2 island — the file's
pragma pushes are a minefield; anchor on the right pop.) Also fixed
en route for Forth's sake earlier today and now doubly load-bearing:
`fopen()` on a directory no longer masquerades as "bad file".

Verified end-to-end in headless: `cpm` → CCP banner → `dir`, `type
readme.txt`, `Bdos Err on D: Select` (no D folder — correct), `save 1
test.com` writing a real host file, `exit` → "the Tube co-processor
has left." → shell intact. /.HELP gained a **tongues:** line — the
machine now speaks EhBASIC, BBC BASIC, Forth and CP/M. 10 suites
green; commit `628c5f7`, both remotes. Parked: a CP/M software drive
(Zork, Turbo Pascal, WordStar need fuller VT emulation in the console
for the screen-oriented ones — line-mode programs work today).

### Addendum (q): the system disk

Doc remembered right: RunCPM ships `DISK/A0.zip` — 81 files, 898 KB,
the whole 1980s in one folder: DRI's ASM, MAC, DDT, ZSID, STAT, PIP,
ED, SUBMIT (patched per DRI's own fixes), the BDOS and CCP *source
code*, Z80ASM with its PDF manual, XMODEM, LU/UNARC/USQ/UNZIP, the
ZEXDOC/ZEXALL Z80 exercisers — and **MBASIC**. Unzipped into
`fs/CPM/` on the laptop and verified live: `STAT DSK:`, a full `D`
listing, then Microsoft BASIC-85 Rev. 5.29 printing squares over the
Tube. The machine now runs five language implementations across
three CPU architectures. The disk itself stays out of the public
repo (mixed provenance; `fs/CPM/.gitignore` documents the one-line
install), commit `d53981c`.

## 2026-08-24 (r) — the guide catches up with the machine

Doc asked for the User's Guide to continue in the established format
(A5 XeLaTeX, Neo6502-handbook style, every screenshot captured from
the running machine at build time — a shot that cannot be produced
fails the build). The shell chapter was rewritten for the current
K/OS (REXX rule with SAY, /!BOOT, dot-hidden files, XD/HEX, WOZ,
HUSH), chapter 3 became "EhBASIC", and three new chapters joined the
User's part: **The Tube: BBC BASIC** (with a live MODE 2 KALEID
screenshot), **Forth** (the session shot ends with the Tali kernel
disassembling itself, `?` at the LDZ byte), and **CP/M: the Z80
Second Processor** (the CCP's DIR over the system disk). 24 pages,
zero overfull boxes after \emergencystretch went into the style.

Build-machine archaeology: k4510-build on ubuntu-s1 has no cc65 and
no nasm, so rom/kernal.bin was a stale leftover (the first shot run
quietly used a pre-Forth ROM) and tube/bbcbasic didn't exist (the
BBC shot showed "the Tube co-processor has left"). Fixed by scp-ing
the ROM, fs and bbcbasic binary from the laptop; also untracked
cpm/runcpm, which an earlier `git add -A` had committed (build
product, host-arch binary, public repo). Commits a245b59..feccfff +
style fix; PDF delivered to Doc.

### Erratum (r), Doc's catch

The guide (and two README sites) taught `@DIR` as the way to reach the
shell from BASIC. The ruling on record is the reverse: `*` is the
universal OS-command prefix in both BASICs — BBC heritage — and `@` is
only an EhBASIC-side alias, a survivor. Fixed as `e7a6462`; the guide
now teaches `*DIR` and footnotes `@`. (Also: this log entry's
predecessor was committed into the wrong repo by a wandering working
directory — moved here, build checkout reset clean.)

## 2026-08-24 (s) — the diary moves in with the code

Doc: "keeping the docs in the main repo is a better idea for now." So
this file, the design doc, VICKE-SPEC, the decision records, FEATURES
and the curated images now live in `docs/` of the k4510 repo itself —
versioned and pushed with the machine they describe. The old
standalone docs repo on ubuntu-s1 stays behind as a local archive
(with its 71 commits of history, the 80 MB of full-resolution phone
photos in screenshots/, and the guide build area). Errata session
earlier tonight, all Doc's catches: * not @ as the BASIC shell prefix,
Memory size ? gone from EhBASIC's boot, the CP/M screenshot that was
secretly photographing Forth's README (make-shots now refuses to run
without its co-processors), and a knowing note on the 3 GHz Z80.

## 2026-08-24 (t) — install.sh, twice

Doc: the SD-card zip "will not be obvious to many" — and he is right,
because the trap is real: the Pi boot ROM reads only FAT32, SDHC cards
(<=32 GB) ship FAT32 and just work, SDXC cards (64 GB+) ship exFAT and
produce a silent black screen. So: **install-sd.sh** (unzips onto a
mounted card, refuses exFAT with an explanation; --format mode wipes a
device to FAT32 with belt-and-braces guards — whole-device only, never
the root disk, type-the-name confirmation) and **setup.sh** (desktop:
apt/dnf/pacman deps — gcc, SDL2, cc65, 64tass, nasm — then make all,
the Tube, CP/M, and the 10-suite battery, with honest reporting of
what is missing and what it is needed for). README gained an
Installing section; the guide's chapter 1 gained "Getting one" and an
SDHC aside before "First boot".

### Erratum (t), from Doc's bench

"Any old card is plenty" — no. Doc tried his 2 GB plain SD (pre-HC)
cards: they do not boot the Pi. So the guidance everywhere is now
*SDHC specifically, 4–32 GB*: older SD fails (field-tested), newer
SDXC ships exFAT and needs the --format treatment. Goldilocks cards.

## 2026-08-24 (u) — sideways ROM and RAM under the I/O page

Doc: "couldn't we move the real roms to far memory and just keep jump
tables in the main 64k?" — which is the BBC Master's sideways ROM,
rediscovered from first principles. Built tonight, both halves:

**Sideways ROM.** The ROM file is now the 24 KB base plus appended
8 KB banks living at phys $0FF00000; the $A000-$BFFF window pages
them via bank register 5. kernal.c was resegmented: resident core
(console, fs, dispatch, runtime, RODATA) in $C000-$CFFF + the $E000
half; cold commands in the window (bank 0 = the base image, no shim
needed); INFO and TIME moved to **bank 1** as proof, called through a
5-line sw_call() shim. The stub page needed zero changes — rom_push
already banks 5-7 off around every system call, so syscalls from
sideways code, and star-commands from inside EhBASIC reaching bank 1,
just work (verified: *TIME and *INFO from a running BASIC, PRINT 6*7
still 42 after the round trips). One cc65 landmine: string literals
pool at end of file and ignore rodata-name pushes unless
**--local-strings** is set — without it every "moved" command left
its strings behind. The famine is over: bank 0 has 660 B spare,
bank 1 has 4.3 KB, resident halves ~2.6 KB combined, and 15 more
banks are one append away. INFO now cheerfully reports its own bank
engaged (banks: 5=$0FF00000) while it runs.

**RAM under the I/O page (K-06).** A MAP of block 6 hides $D000-$DFFF
and exposes RAM — MAP is an instruction, so no register deadlock; the
bank-register view keeps I/O on top (SID players unaffected), and the
$D000 address never moves (199 C64 tunes hardcode $D400). With ROM
banked away too: 61.75 KB contiguous. maptest grew case K-06.

Also: CPM's greeting no longer claims to be BBC BASIC. Parked, next
stage of the pinned plan: user space to $CFFF by default and EhBASIC
relocated ("38911 BASIC BYTES FREE"). All 10 suites green.

## 2026-08-24 (v) — stage 3: the payoff. 45567 BASIC BYTES FREE

The memory plan's payoff stage, Doc's "ok do it". Programs now own
$0800-$CFFF and $E000-$FEFF **by default**: run_at() builds a 28-byte
trampoline in RAM at $02D8 that engages banks 5-7 onto the RAM under
the ROM (skipping blocks a K4SG load claimed), calls the program, and
turns every bank off on return. It must be RAM: the instant block 7
engages, the ROM half that built it is gone. Which was also the
evening's one real crash — the setup code used w32() on the bank
registers, whose byte-3 write IS the engage trigger, so run_at
vaporised its own ROM mid-flight (Z ended up 1 and the console spent
a while printing invisibly into the attribute plane — a new entry for
the phantom-crash genre). Byte 3 now stays in the trampoline, and the
K4SG loader got the same discipline for blocks 5-7.

EhBASIC moved above BASIC's RAM: three K4SG segments — $E000-$FEFF
(7839 bytes, cut at LAB_NLTO after an automated search for a boundary
no relative branch crosses), $C000-$CFFF (core tail + math + the
expression compiler), and $BA00-$BFFF (file + gfx glue) in the RAM
under the sideways window. Ram_top = $BA00 and the machine boots:
**45567 Bytes free** (was 25599). DIM A(9000) really allocates 36 KB;
demos, graphics, LOAD/SAVE, star commands, sidplay, sprites all
green. Tali Forth's image moved to $8C00: the dictionary grew from
12.8 to ~31.7 KB. The C64's 38911 is beaten without an asterisk.

Also on the record (relayed from Doc via the archive-side session):
**msbasic is coming** — Microsoft's MIT-licensed 6502 BASIC via
mist64/msbasic, eventually beside/replacing EhBASIC (whose
non-commercial licence is the murkiest thing in the ROM). Sideways
banks 2-3 are earmarked for it, though stage 3 opens a second door:
it could live EhBASIC-style in high K4SG segments instead. Decision
when it's ported; entry (w) below carries the full research.
## 2026-08-24 (w) — the BASIC research: what could replace EhBASIC (mirrored from the archive log, commit a7ccffc)

Discussion session (this host, archive folder — the coding session was
busy reshaping the sideways-ROM map and was left alone). It started as
"could adding elements of the 8510 give us 128k ram?" and ended, three
questions later, at "research other BASICs for the 4510 side." Along
the way: the 8510 is just an HMOS 6510 — the C128's 128K was the
**8722 MMU**, not the CPU; its `122365 BYTES FREE` was two 16-bit
pools summed through common-RAM trampolines; and the Tube BBC BASIC
already owns 250 MB, so the native side is the open question.

Two web-research agents ran in parallel: one on the Commodore lineage,
one on independent 6502 BASICs. **Outcome, decided by Doc the same
day: bring Microsoft 6502 BASIC (MIT since Sept 2025) into play;
the coding session was asked to reserve a sideways-ROM slot.** The
full reports follow, pasted verbatim for the record.

---

### Report 1 — Commodore-lineage BASIC options (45GS02-native)

#### The headline finding
**Microsoft open-sourced 6502 BASIC v1.1 (1976-78) under MIT in
September 2025** — github.com/microsoft/BASIC-M6502 (repo archived
read-only 2025-09-05, license stands). This resets the legal landscape
for everything MS-lineage.
([Hackaday](https://hackaday.com/2025/09/04/microsoft-basic-for-6502-is-now-open-source/),
[Tom's Hardware](https://www.tomshardware.com/software/bill-gates-48-year-old-microsoft-6502-basic-goes-open-source))

#### 1. C65 ROM BASIC 10 (910111)
- **License: PROPRIETARY** — all Commodore ROMs owned by Cloanto; the
  C65 ROM is licensed to MEGA65 per-device (fee per unit sold) and
  bundled in C64 Forever since 2022. No standalone redistribution
  right. ([generationamiga.com](https://www.generationamiga.com/2020/04/09/cloanto-licenses-commodore-65-rom-for-the-upcoming-mega-65/),
  [forum64 thread](https://forum64.de/index.php?thread%2F106568-about-the-license-of-the-c65-rom=))
- **Source:** never released; only community disassemblies
  (zimmers.net hosts binaries — legally grey to even mirror).
- **>64K:** yes, natively — program text in bank 0, variables/strings
  in bank 1 (the model BASIC65 inherited); graphics commands
  DMA-backed.
- **Porting: HIGH** — binary-patch-only (no legal source), deeply
  entangled with VIC-III, C65 F011 DOS, CIAs, C65 Kernal jump table.
  Also famously buggy (prototype ROM).
- **Footprint:** part of the 128 KB system ROM (BASIC+editor+graphics
  roughly half; Kernal/DOS/charset the rest).
- **Verdict for a public repo: unusable.**

#### 2. MEGA65 BASIC65
- **License: PROPRIETARY** — the "closed ROM."
  github.com/MEGA65/mega65-rom-public is a **bug tracker only**: "As a
  MEGA65 owner you have acquired a license to the ROM"; source repo
  access is by request on Discord, owners-only, no
  redistribution/derivative rights for other machines.
  ([mega65-rom-public](https://github.com/MEGA65/mega65-rom-public))
  Underlying Commodore rights still Cloanto's.
- **>64K:** yes, the best of the family. Commands per the MEGA65
  User's Guide ([memory.tex](https://github.com/MEGA65/mega65-user-guide/blob/master/memory.tex)):
  - `BANK n` — sets the bank used by `PEEK/POKE/SYS/BLOAD/BSAVE` etc.;
    with BANK 0–5 or address >$FFFF, PEEK/POKE go through the
    45GS02's base-page quad-pointer ([addr32],Z) far addressing —
    i.e. real 28-bit PEEK/POKE.
  - `DMA` — C65-style F018 DMA job from BASIC; `EDMA` — enhanced DMA
    with full 28-bit flat source/dest addresses.
  - Extended `PEEK, POKE, SYS, BLOAD, BVERIFY, SCREEN, SPRSAV…` are
    all bank-aware. Known limit: BANK/DMA can't reach colour RAM
    beyond the first megabyte.
  - Program/variable model: bank 0 text + bank 1 variables (BASIC 10
    heritage).
- **Porting: HIGH** — even ignoring the license wall: tied to VIC-IV
  registers, MEGA65 hypervisor traps, C65-DOS/SD controller, CIAs.
  The license alone disqualifies it.
- **Footprint:** 128 KB ROM total (BASIC is the majority).
- **Verdict: the feature model to imitate, not code to take.**

#### 3. MEGA65 Open-ROMs
- **License: CLEAN (GPLv3)** — documented clean-room process (spec
  from books like Mapping the C64, similarity tool flagging >2-byte
  matches against originals).
  ([open-roms](https://github.com/MEGA65/open-roms))
- **Completeness (per [STATUS.md](https://github.com/MEGA65/open-roms/blob/master/STATUS.md),
  still true in 2025-2026):** Kernal is substantially done (IEC,
  devices, screen partial); floating-point math package done; **but
  the BASIC interpreter core is not**: integer/float variables/arrays
  NOT DONE, expression handling only partial (strings mostly work),
  FOR/NEXT, GOSUB, IF/THEN NOT DONE. It boots to READY and runs
  trivial things; it cannot run real BASIC programs.
- **>64K:** extended-BASIC ideas exist for MEGA65 builds, but nothing
  bank-aware is usable yet.
- **Porting: HIGH as a working BASIC** (you'd be writing the
  interpreter yourself), **LOW-MED as a parts quarry** (GPL3 math
  package, string GC, Kernal patterns). Note GPLv3 would apply to
  your ROM if you incorporate it — fine for your repo, but it's viral.
- **Footprint:** C64-shaped (8K BASIC + 8K Kernal regions; MEGA65
  build larger).

#### 4. mist64/msbasic
- **License: MURKY-but-now-anchored.** The repo's README claims
  "2-clause BSD" — but that can only cover mist64's reconstruction
  scaffolding; the code itself is Microsoft's (byte-exact rebuilds of
  9 shipped ROMs). **Since Sept 2025 the Microsoft core is genuinely
  MIT** via microsoft/BASIC-M6502. Residual murk: the
  Commodore-specific patch levels (CBM BASIC 1/2 ROM-exact builds)
  contain **Commodore-authored modifications** that the MIT grant
  doesn't cover and Cloanto still claims.
- **Practical combo:** microsoft/BASIC-M6502 is the *legal anchor* but
  is written in PDP-10 MACRO-10 syntax; msbasic is the *buildable
  equivalent* — ca65/cc65 toolchain, same one your ROM already uses.
  Build a generic/OSI/KBD-style config (pure-MS feature set), avoid
  the CBM-ROM-exact targets, and cite the MS MIT release in your
  license table.
- **>64K:** no — 16-bit pointers throughout; strings, arrays, program
  text all in one 64K image. You'd bolt on far PEEK/POKE/DMA tokens
  yourself (your 45GS02 [zp],Z far pointers make the *data* side
  easy; making program/variable storage >64K is a rewrite).
- **Porting: LOW** — designed for retargeting: platform config files,
  I/O isolated to a handful of vectors (char in/out, ctrl-C check,
  LOAD/SAVE hooks). Zero VIC/CIA/Kernal entanglement.
- **Footprint:** ~8-9 KB.
- **Source:** github.com/mist64/msbasic +
  github.com/microsoft/BASIC-M6502.

#### 5. Commander X16 ROM BASIC
- **License: MIXED, core is PROPRIETARY.**
  [LICENSE.md](https://github.com/X16Community/x16-rom/blob/master/LICENSE.md)
  is explicit: the `basic` and `math` directories are "©1977 Microsoft
  Corp. ©1983 Commodore Business Machines" under a **commercial
  license from Cloanto valid only "in the context of the X16
  computer"** — outside users are told to contact Cloanto. Only the
  *additions* (new BASIC commands, CMDR-DOS, FAT32, audio API) are
  BSD-2, plus GPLv3 open-roms Kernal pieces. So no, it is not clean
  for reuse, and the community is honest about it.
- **>64K:** partial — `BANK` selects an 8 KB RAM bank at $A000 (up to
  2 MB) for PEEK/POKE/SYS; `BLOAD` auto-wraps across banks. Program +
  variables still live in the fixed low-RAM ~38 KB.
- **Porting: MED** technically (C64-BASIC-shaped, X16 banking
  hardware assumptions) — but license-blocked for the core; only its
  BSD-2 extension code (graphics/audio command implementations, DOS
  wedge) is reusable.
- **Footprint:** ~16 KB ROM bank + annex bank for the extensions.

#### Summary table (report 1)

| Candidate | License verdict | >64K | Porting | Footprint |
|---|---|---|---|---|
| C65 BASIC 10 (910111) | PROPRIETARY — Cloanto, per-device MEGA65/C64-Forever only | Yes (bank0 text/bank1 vars) | HIGH (no source, VIC-III/DOS-welded) | ~half of 128 KB ROM |
| MEGA65 BASIC65 | PROPRIETARY — owners-only source, Cloanto underneath | Yes — BANK, 28-bit PEEK/POKE, DMA, EDMA | HIGH (license + VIC-IV/hypervisor) | 128 KB ROM |
| Open-ROMs | CLEAN — GPLv3 clean-room | Not yet | HIGH as BASIC, LOW as parts quarry | ~16 KB |
| msbasic + MS MIT release | CLEAN for pure-MS configs; MURKY for CBM-ROM-exact builds | No (16-bit; far tokens bolt-on) | LOW — vector-isolated I/O, ca65/cc65 | ~8-9 KB |
| X16 ROM BASIC | PROPRIETARY core (Cloanto, X16-context-only); BSD-2 additions | Partial (BANK + 8K window) | MED, license-blocked | ~16-32 KB |

#### Other options found (report 1)
- **C128 BASIC 7.0 / Plus-4 BASIC 3.5** — the split-pool (bank-0 text
  / bank-1 vars) 7.0 model: same Cloanto-proprietary status as BASIC
  10; original Commodore engineering sources circulating on
  zimmers.net are leaks, not licensed. Dead end for a public repo.
- **microsoft/BASIC-M6502 as a direct base** — worth listing
  separately from msbasic: it is the one genuinely MIT-licensed
  Commodore-ancestor BASIC in existence (the exact code the PET ran).
  Legal gold; needs MACRO-10→ca65 translation, which msbasic has
  effectively already done.
- **A license-table caveat you already carry: EhBASIC.** Lee
  Davison's EhBASIC is "free for non-commercial use" only, and
  Davison died in 2013 leaving the rights orphaned — it is arguably
  the murkiest thing in your current ROM. The pragmatic clean-up path
  given everything above: migrate the native BASIC to an
  msbasic/BASIC-M6502-derived build (MIT), then add your own
  BANK/EDMA/far-PEEK-POKE tokens modeled on BASIC65's command
  surface.

---

### Report 2 — Non-Commodore-lineage 6502 BASICs

#### 1. EhBASIC 2.22 (Klaus2m5 fork) — the incumbent
- **License (exact):** Not OSI. "EhBASIC is free but not copyright
  free" — non-commercial use OK provided binaries/docs carry "Derived
  from EhBASIC"; commercial use required contacting Lee Davison, who
  died in 2013, so the commercial clause is now un-clearable and the
  license can never be regularized. Widely redistributed anyway
  (Klaus2m5, jefftranter, lgblgblgb forks all public on GitHub);
  community treats it as tolerated with attribution.
- **Implementation:** 6502 assembly (single source; many ports to
  ca65).
- **Memory model:** Classic 16-bit pointers everywhere (program,
  vars, strings all in one 64K image). No fork with banked/large
  memory was found — the only sighting is the PZ1 laptop running
  stock EhBASIC *under* a banking scheduler (banking is outside
  BASIC). **>64K: TEACHABLE** — far PEEK/POKE/COPY tokens using
  45GS02 [zp],Z is easy; making program/variable storage itself >64K
  is major surgery on ~16-bit pointer code throughout.
- **REPL:** Y. **Port effort: NONE** (already running).
  **2025-26 activity:** frozen (Klaus fork ~18 commits, bugfix-era).
- URL: https://github.com/Klaus2m5/6502_EhBASIC_V2.22

#### 2. BBC BASIC for 6502 (Acorn original + derivatives)
- **License:** Proprietary, orphaned. The 2018 Apache-2.0 RISC OS
  Open release covers **ARM BBC BASIC V only, not the 6502 ROMs**.
  Stardot consensus: the 6502 BASIC IP passed Acorn→Element
  14→Pace→Castle→(RISC OS Developments) so many times that "not even
  Sophie Wilson is sure who owns it now." J.G. Harston's assemblable
  source on mdfs.net still carries Acorn copyright; the Tube "BASIC V
  for 65C02" there is copyright Colin C Dean, also not free.
  BeebEater (github.com/chelsea6502/BeebEater, for Ben Eater builds)
  is a nice MIT wrapper but **ships the proprietary ROM binary
  inside**.
- **>64K:** NO (16-bit; the BBC's answer was the Tube/sideways RAM,
  outside BASIC). **REPL:** Y. **Port effort:** LOW technically (MOS
  entry-vector shims, BeebEater proves it) but **legally unusable for
  a clean public repo**. No open-source reimplementation of 6502 BBC
  BASIC exists as of 2026.
- URLs: https://mdfs.net/Software/BBCBasic/6502/ ,
  https://stardot.org.uk/forums/viewtopic.php?t=16087 ,
  https://github.com/chelsea6502/BeebEater

#### 3. FastBasic (dmsc) — strongest real candidate
- **License (exact):** GPL-2.0-or-later **with an explicit linking
  exception**: programs you compile with it may be distributed under
  any license. Clean for a public repo.
- **Implementation:** Parser/compiler in 6502 asm (on-machine IDE) +
  C++ cross-compiler on PC; runtime is a small bytecode **VM in ca65
  assembly** — it already builds with the cc65 toolchain the K4510
  uses.
- **Memory model:** 16-bit VM; 16-bit ints + Atari-ROM floating point
  (FP routines would need replacing or dropping for a port).
  **>64K: TEACHABLE, and more cheaply than anywhere else** — because
  all memory access funnels through ~10 VM opcodes, adding
  FARPEEK/FARPOKE/FARCOPY (or even a far string pool) means touching
  the VM, not a whole interpreter. The 45GS02's [zp],Z 32-bit mode
  slots straight into a VM opcode.
- **REPL:** Y-ish — full-screen IDE with editor + instant
  compile-and-run on the machine (not line-at-a-time immediate mode).
- **Port effort: MED** — author states "the libraries are fairly
  portable, so creating a version for other 6502s shouldn't be too
  much work"; work = console I/O layer, replace Atari FP, strip
  P/M-graphics statements. **Activity:** active — v4.6 (2024), v4.7
  discussed on AtariAge; 866 commits.
- URL: https://github.com/dmsc/fastbasic

#### 4. XC=BASIC 3 (neilsf)
- **License:** MIT. **Implementation:** cross-compiler written in
  **D** (runs on PC, emits 6502 via DASM). **REPL: N — compile-only**,
  which fails the core requirement. **>64K:** NO (16-bit codegen);
  teachable only by hacking the D codegen. **Port effort:** MED (add
  a target config + runtime shims; C64/VIC20/C16/Plus4/PET/C128
  supported, X16 via community target). **Activity:** mature, ~970
  commits, slow but alive.
- URL: https://github.com/neilsf/xc-basic3

#### 5. Tiny BASICs (Tom Pittman, DDJ-IL, CorshamTech, uBASIC etc.)
All are 2-8 KB toys with 16-bit (or 8-bit!) address spaces, no
strings/FP worth having, and nothing to say about large memory —
strictly a step down from EhBASIC. (CorshamTech's is GPL-3 and
maintained if a minimal fallback is ever wanted:
https://github.com/CorshamTech/6502-Tiny-BASIC ; uBASIC is BSD, in C,
cc65-compilable, and would run — but it's if/for/goto-only. The irony
is affordable, the language isn't.)

#### 6. dflat (6502Nerd)
- **License:** MIT. **Implementation:** 6502/65C02 assembly.
  **REPL:** Y (interactive, structured BASIC-like: def/enddef
  procedures, locals, recursion, while/repeat — no GOTO). **>64K:**
  NO as-is; TEACHABLE same as EhBASIC (it's conventional 16-bit asm
  inside). **Port effort: MED-LOW** — explicitly designed to port:
  "core language just needs character put and get routines."
  **Activity:** ongoing hobby refinement (Oric-1 branch, ~89
  commits); one-man project, non-standard dialect (existing BASIC
  listings won't run).
- URL: https://github.com/6502Nerd/dflat

#### 7. BASIC816 (pweingar, C256 Foenix) — the 65816 design reference
- **License:** **GPL-3.0** (author has said he'd consider MIT if
  asked). **Implementation:** 65816 assembly (64tass), clean-room,
  interactive REPL, shipped as the stock BASIC of the C256 Foenix U
  (a 65816 machine with up to 4 MB flat RAM). **>64K: YES —
  genuinely** — program text, variable table and string heap
  addressed with native 24-bit long pointers; this is the
  proof-of-existence for "an interactive BASIC whose *heap* lives
  above bank 0."
- **Not portable to the 45GS02** (65816 native mode ≠ 45GS02;
  opcode/register model differs), so **port effort: HIGH / treat as
  reference only**: what it did — keep interpreter code+ZP state in
  bank 0, use long-pointer addressing modes for every data structure
  — maps directly onto the 45GS02's [zp],Z 32-bit pointers.
  **Activity:** mostly 2019-2021; README still calls itself unstable
  though it ships on real hardware. No comparable Apple IIGS/SNES
  open BASIC found (GS BASICs are Apple-proprietary).
- URL: https://github.com/pweingar/BASIC816

#### 8. Wildcard that changed the landscape: Microsoft 6502 BASIC, MIT (Sept 2025)
Technically Commodore-lineage (it *is* the ancestor), but note:
Microsoft released the original 6502 BASIC 1.1 source (m6502.asm,
~6,955 lines, the pagetable multi-target source) under **MIT** on
2025-09-03. If the owner ever wants a license-spotless classic
Microsoft-style core to hack far-memory features into — with zero
attribution ambiguity — this now exists and EhBASIC's grey clause
stops being the only game in town.
https://opensource.microsoft.com/blog/2025/09/03/microsoft-open-source-historic-6502-basic/

#### 9. BASIC-in-portable-C compiled with cc65
Feasible but poor: uBASIC (BSD, dunkels.com/adam/ubasic/) compiles
under cc65 and would fit (~few KB code), but cc65-generated
interpreter code is 3-5x the size and far slower than hand asm, and
every richer C BASIC (MY-BASIC, etc.) blows the 24 KB ROM-bank budget
and assumes malloc/heap ≫ what's free. Verdict: only worth it for a
toy scripting sublanguage, never as *the* BASIC.

#### Comparison table (report 2)

| Candidate | License | >64K | REPL | Port | Note |
|---|---|---|---|---|---|
| EhBASIC (Klaus2m5) | non-commercial + attribution, un-clearable | TEACHABLE | Y | NONE | already running; far PEEK/POKE easy, far heap = surgery |
| BBC BASIC 6502 | proprietary, orphaned IP | NO | Y | LOW (tech) / blocked (legal) | ARM BASIC V is Apache-2.0, 6502 ROMs are not |
| FastBasic | GPL-2.0+ w/ linking exception | TEACHABLE (via VM opcodes) | Y (on-machine IDE) | MED | ca65-based, active 2025, best asm-effort/feature ratio |
| XC=BASIC 3 | MIT | NO | N | MED | compile-only kills it for this use |
| Tiny BASICs | varies | NO | Y | LOW | too small to matter |
| dflat | MIT | TEACHABLE | Y | MED-LOW | portable by design, one-man non-standard dialect |
| BASIC816 | GPL-3.0 | YES (24-bit long ptrs) | Y | HIGH (ref only) | the blueprint for a far-heap interactive BASIC |
| MS 6502 BASIC (2025 MIT) | MIT | TEACHABLE | Y | LOW-MED | Commodore-lineage but now license-clean |
| uBASIC via cc65 | BSD-3 | trivially (C far shims) | Y | LOW | too weak a language |

#### Top 3 for the BMC-K4510 (report 2's ranking)
1. **FastBasic** — the only active, license-clean (GPL2+exception),
   cc65-toolchain-native candidate where >64K support is
   architecturally cheap: add far-memory opcodes to a small bytecode
   VM using the 45GS02's [zp],Z pointers, rather than re-plumbing an
   entire interpreter. On-machine IDE satisfies "interactive."
2. **Keep EhBASIC, teach it far ops** — zero porting cost, users
   already have it; add FARPEEK/FARPOKE/FARBLOCK/bank-pool tokens
   (ROM budget permitting). Accept the grey "Derived from EhBASIC"
   non-commercial clause — fine in practice for a hobby public repo,
   but it can never be made properly open.
3. **dflat** — MIT, explicitly built to be ported to homebrew 6502
   machines, structured and interactive; the fallback if FastBasic's
   Atari FP/runtime extraction proves heavier than expected. Use
   **BASIC816** as the design reference for any far-heap work
   regardless of which core wins.

---

### The synthesis and the decision

Ranked across both reports for this machine: **migrate the native
BASIC to an msbasic/BASIC-M6502 build (pure-MS config, MIT anchor),
then add our own BANK / 28-bit PEEK-POKE / DMA tokens modeled on
BASIC65's command surface** — clean license table, authentic
Commodore-family dialect (EhBASIC is an MS-alike, demos mostly carry
over), ~8-9 KB core, far commands in code we fully own. The banner
can then print a C128-style number the C128's own way (pool
arithmetic) — backed by commands that really reach the 256 MB.
FastBasic stays the runner-up; BASIC816 the blueprint if a true
far-heap BASIC ever goes on the ballot.

Doc's call, same evening: do it. The coding session (then reworking
the sideways-ROM memory map) was messaged to reserve a slot for the
msbasic ROM plus token headroom; the vendoring itself is a later work
item. Decision also recorded in project memory
(`project-k4510-msbasic-decision.md`).
## 2026-08-24 (x) — the font research: a clean chargen (mirrored from the archive log, commit 9ea9086)

Same discussion session, next question from Doc: "research other 8x8
bitmapped fonts that are license compatible? ascii and petscii." The
motivation is the same trap the BASIC research walked around — the
original C64/C65 chargen ROMs are Commodore IP, Cloanto-claimed, the
same category as the BASIC ROMs. Two web-research agents ran in
parallel: one on PETSCII-flavored/Commodore-style fonts, one on
general open 8x8 ASCII faces. Full reports pasted verbatim below.

**The short version:** the drop-in winner is the MEGA65 open-roms
clean-room chargen (`bin/chargen_openroms.rom` — 4 KB, 512 glyphs,
both PETSCII sets, already in 8-bytes-per-glyph format, LGPL-3.0);
**unscii-8** is the public-domain powerhouse (complete Legacy
Computing/PETSCII repertoire, trivial .hex conversion, but the
512-glyph layout must be assembled by hand); **BESCII** (CC0) is the
most C64-flavored clean design. The prettiest matches — Pet Me and
Style64's C64 TrueType — are both license-excluded. And the legal rule
of thumb the community follows: *inspired-by with visible pixel
differences = safe; pixel-identical to the Commodore ROM = same bytes
as the ROM = don't put it in a public repo.*

---

### Report 1 — PETSCII-flavored 8x8 fonts with clean licenses

#### 1. Kreative Korp "Pet Me" family — NOT usable
- **Page:** https://www.kreativekorp.com/software/fonts/c64/ (the
  `/petme/` URL 404s; download:
  https://www.kreativekorp.com/swdownload/fonts/retro/petme.zip)
- **Coverage:** FULL, best-in-class — Pet Me (PET), Pet Me 2X
  (VIC-20), Pet Me 2Y (CBM2/80col), Pet Me 64/64 2Y, Pet Me 128/128
  2Y. "code points 0xE000-0x1FF encode the complete Commodore 64
  character set"; also mapped to Symbols for Legacy Computing since
  Oct 2019.
- **Format:** TrueType only.
- **License:** "Kreative Software Relay Fonts Free Use License
  v1.2f" — full text at
  https://www.kreativekorp.com/software/fonts/FreeLicense.txt. Free
  redistribution with credit is allowed (clause 1a), **but clause 2
  is fatal**: *"The User may not modify, reverse-engineer, or create
  any derivative works of the Software."* Converting the TTF into
  8-byte-per-glyph ROM data is a derivative work. Also clause 5:
  *"Kreative Software reserves the right to change this license at
  any time without notice."* **Verdict: excluded for ROM
  conversion.** (These fonts are also pixel-exact traces of the
  original ROMs, so the deeper Cloanto question below applies too.)

#### 2. Style64 "C64 TrueType (Pro)" — NOT usable
- **License page:** https://style64.org/c64-truetype/license
- Quoted terms: *"You MAY NOT: sell this font; include/redistribute
  this font in any font collection…; provide the font for direct
  download from any web site."* Embedding permitted only *"without
  any modification and using the same filenames"* (web @font-face),
  or *"as part of a software package but ONLY if said software
  package is freely provided to end users."* No modifications
  allowed in any case; anything more requires negotiating *"a
  (possibly commercial) license."*
- **Verdict: excluded.** No-modification + no-direct-download
  clauses are incompatible with converted ROM data sitting in a
  public git repo. Their https://style64.org/petscii/ page is still
  useful as a *reference* — it defines the "Direct PETSCII" PUA
  mapping (U+E000/E100/E200/E300 banks) but maps to PUA, not to
  U+1FB00, and offers no downloadable table.

#### 3. MEGA65 open-roms — CONFIRMED, ready-made ROM data
- **Repo:** https://github.com/MEGA65/open-roms — license per
  `LICENSE`: **LGPL v3 or later** (not plain GPLv3), copyright
  Gardner-Stephen / Standzikowski; some BASIC files MIT (Microsoft).
- **Charset exists and is complete:** `assets/8x8font.png` (8×4096
  px = 512 glyphs = **both charsets, 2×256, full PETSCII
  graphics**), built by `pngprepare` into
  `bin/chargen_openroms.rom` — a prebuilt **4096-byte chargen ROM,
  already in the exact 8-bytes-per-glyph format** needed. It is a
  distinct clean design, not a pixel copy of the Commodore ROM.
- Bonus: `bin/chargen_pxlfont_2.3.rom` (4 KB, also drop-in chargen
  format) — "PXLfont 88665b RF2.3" by Retrofan; `bin/README.md`
  states: *"PXL font was created by Retrofan, we got a permission to
  include it with Open ROMs under GNU Lesser General Public License
  3.0."* (Outside open-roms, PXLfont's own terms are
  permission-required — e.g. the Ozmoo copy at
  https://github.com/johanberntsson/ozmoo/blob/master/fonts/PXLfont-rf.license.txt
  is Ozmoo-only — so take it *via* open-roms under LGPL-3.0.)
- **Verdict: usable.** LGPL-3.0 on a 4 KB data blob is the only
  cost; for chargen data used as data (not linked code), LGPL's
  obligations reduce to shipping the license + source (the PNG).

#### 4. unscii-8 (Viznut) — CONFIRMED public domain, near-complete
- **Page:** http://viznut.fi/unscii/ (note: expired/invalid TLS cert
  as of 2026-08; content intact)
- **License:** quoted from the page: *"'unscii-16-full' falls under
  GPL because of how Unifont is licensed; **the other variants are
  in the Public Domain**."* So unscii-8 = public domain, no
  conditions.
- **Coverage (verified by downloading `unscii-8.hex`):** 3191
  glyphs; **213 glyphs in U+1FB00 Symbols for Legacy Computing**
  (Unicode 13 added 214 — effectively complete, and the page
  explicitly says the block includes "the missing PETSCII
  characters"); 256 glyphs across U+25xx (box drawing, block
  elements, geometric); card suits at U+2660/2663/2665/2666 all
  present. Full PETSCII repertoire reachable for both upper/graphics
  and lower sets via ASCII + these blocks; also ships `uns2uni.tr`
  (PUA↔Unicode mapping file).
- **Format/conversion:** HEX (Unifont hexdump — for 8x8 each line is
  codepoint + 16 hex digits = **exactly 8 bytes/glyph**; conversion
  is a 10-line script), plus PCF/TTF/OTF/WOFF. **Verdict: easiest
  and cleanest license of all; style is unscii's own, not
  Commodore-look.**

#### 5. CC0/MIT PETSCII-inspired fonts — two verified
- **BESCII** (Damian Vila) —
  https://github.com/damianvila/font-bescii (archived; moved to
  https://codeberg.org/Dmian/font-bescii). `LICENCE` file verified:
  **CC0 1.0 Universal** full legal text. README: "An 8x8 pixel font
  based on PETSCII… PETSCII symbols + some Amstrad CPC 464… PETSCII
  characters mapped using Direct PETSCII mapping" (style64 PUA
  scheme), plus Latin/Greek/Cyrillic/kana. **Coverage: full PETSCII
  graphics repertoire, deliberately *not* pixel-identical** (a
  redesign fixing C64 font flaws — see
  https://damianvila.com/blog/20240515-designing-the-bescii-font.html).
  Format: TTF/OTF/WOFF/WOFF2 **and FontForge .sfd source** (v2.0:
  `Bescii-Mono.sfd`) — conversion needs rasterizing the TTF at 8px
  or parsing the .sfd; moderate effort.
- **funscii** (Wuerfel21) — https://github.com/Wuerfel21/funscii.
  Verified: repo SPDX **CC0-1.0**; README: *"The font itself is put
  into the public domain - licensed under the terms of CC0 1.0
  Universal"* (builder is Apache-2.0). It is a fork of unscii with
  fixes + Japanese; source `font.txt`/`glyphs` in unscii's text
  format; community reports a C64-style binary build. Same coverage
  story as unscii-8.
- FontStruct "PETSCII Commodore"
  (https://fontstruct.com/fontstructions/show/1336244/petscii-commodore)
  is tagged CC0 but is a pixel-copy traced from the Wikipedia
  PETSCII chart — the uploader cannot launder the original bitmap
  into CC0; treat as unsafe.

#### 6. Unicode Symbols for Legacy Computing as mapping target — CONFIRMED
- Block U+1FB00–U+1FBFF, added in Unicode 13.0 (2020) specifically
  for PETSCII et al. Proposal **L2/19-025** ("Proposal to add
  characters from legacy computers and teletext", successor of
  L2/17-435):
  https://www.unicode.org/L2/L2019/19025-terminals-prop.pdf —
  **contains per-machine mapping tables (incl. Commodore PETSCII →
  Unicode)** that can drive a conversion; supplement L2/21-235
  (Unicode 16 additions) at
  https://www.unicode.org/L2/L2021/21235-terminals-supplement.pdf.
- Human-readable PETSCII→Unicode mapping table:
  https://www.kreativekorp.com/charset/map/petscii/
- Open 8x8 fonts implementing the block: **unscii-8** (213/214
  glyphs, verified), **funscii**, Pet Me 2019+ (license-blocked),
  Kreative's Fairfax HD (OFL, but 6x12 not 8x8).

#### 7. Other findings
- **VICE fallback charset:** none exists — VICE (GPLv2+) ships the
  *original* Commodore `chargen` ROM images on the old Usenet-era
  tolerance; the ROMs are not GPL and are exactly the
  Cloanto-claimed material (community discussion:
  https://www.lemon64.com/forum/viewtopic.php?t=73857). Nothing to
  reuse.
- **ZX Origins (DamienG):** ~hundreds of 8x8 fonts incl. "C64"
  export formats (C headers, 6502 asm); terms are informal —
  "freely available… in exchange for a mention in the credits"
  (https://damieng.com/typography/zx-origins/). ASCII-96 only, **no
  PETSCII graphics repertoire**; useful for alternate text glyphs,
  not for the chargen graphics half.

#### Ranked top 3 (report 1)
1. **MEGA65 open-roms `chargen_openroms.rom`** — already a 4 KB,
   512-glyph, 8-bytes-per-glyph chargen with both PETSCII sets,
   drop-in zero conversion; LGPL-3.0 (note in the license table).
   PXLfont 2.3 from the same `bin/` dir is a nicer-looking second
   option under the same license.
2. **unscii-8** — public domain, no strings at all; `.hex` converts
   trivially; complete Legacy Computing/PETSCII glyph repertoire,
   but you must build the 512-entry PETSCII layout yourself using
   the L2/19-025 mapping, and the look is unscii's, not Commodore's.
3. **BESCII** — CC0, deliberately C64-flavored (closest "feel" with
   a clean pedigree), full PETSCII graphics via Direct-PETSCII PUA
   mapping; needs TTF→bitmap extraction (it is a true 8x8 grid
   design, so 8px rasterization is lossless).

Excluded despite being the prettiest matches: Pet Me
(no-derivatives clause) and Style64 C64 TrueType (no-modification,
no-direct-download).

#### Legal caveat: pixel-exact clones of the Commodore charset
Honest summary: **unsettled, lean away.** Community/legal consensus
(e.g. the Lemon64 threads above): in the US, typeface *designs* are
not copyrightable, but the ROM as a data file is, so byte-copying
the chargen ROM is clearly off-limits; the open question is whether
an independently-typed but pixel-identical 8x8 bitmap is a "copy of
the ROM data" (it is byte-identical by construction) or an
uncopyrightable typeface rendering. No case law answers this for
8x8 chargen bitmaps; jurisdictions differ (UK/Germany protect
typefaces, though 25-year terms have expired for 1982 designs).
Cloanto/C64-forever actively license the ROMs, and open-roms chose
clean-room reimplementation precisely to avoid the argument.
Practical rule the community follows and open-roms/BESCII embody:
*inspired-by with visible pixel differences = safe; pixel-identical
= same bytes as the ROM = don't put it in a public repo.* All three
ranked picks satisfy this.

---

### Report 2 — General open 8x8 ASCII bitmap fonts

#### 1. dhepper/font8x8 — https://github.com/dhepper/font8x8
- **License:** Public domain (stated in repo README). *Caveat:*
  provenance chain is "directly derived from an assembler file" by
  Marcel Sondaar, itself based on "IBM public domain VGA fonts."
  IBM never formally dedicated these to the PD — the claim
  ultimately rests on the US doctrine that bitmap glyph designs are
  uncopyrightable. Community treats it as safe; used everywhere
  (OS-dev tutorials, embedded projects).
- **Coverage:** Basic ASCII (0x00–0x7F), extended Latin
  (0x80–0xFF), box drawing, block elements, Greek, Hiragana — as
  separate C arrays.
- **Format:** C header arrays, exactly 8 bytes/glyph, LSB =
  leftmost pixel. **Zero conversion needed** — already chargen ROM
  format (bit-reversal per byte may be needed depending on shift
  orientation).
- **Readability:** Classic IBM-ish face; lowercase without true
  descenders (CGA-style squash); 0 unslashed but distinguishable
  from O; 1/l/I distinct. Serviceable, very "PC."
- **Status:** Repo dormant (7 commits) but stable.

#### 2. Ultimate Oldschool PC Font Pack (VileR) — https://int10h.org/oldschool-pc-fonts/readme/
- **License:** **CC BY-SA 4.0**. Attribution: credit "VileR" + link
  to int10h.org. Adaptations (which ROM-converted glyph data is)
  must be distributed under a compatible license.
- **8x8 faces in the pack:** IBM CGA 8x8, AMI EGA 8x8, ATI 8x8,
  Verite 8x8, ToshibaTxL1 8x8, and dozens more OEM 8x8s (CGA
  thin/thick, EGA, Amstrad, Phoenix, etc.).
- **Mechanics:** Converting glyphs to a C array/ROM binary is fine
  under CC BY-SA with credit + the CC BY-SA 4.0 notice on the
  derived font data. The share-alike obligation attaches to the
  font data, not to the emulator code that merely loads it (fonts
  as data are generally treated as separate works — convention, not
  litigated certainty). One CC BY-SA row in the license table.
- **Underlying IBM/OEM designs:** VileR's own legal analysis: "The
  raw bitmap typefaces are not copyrightable, unlike fonts in
  specific formats such as .fon and TrueType (which qualify as
  software)" (citing *Eltra Corp. v. Ringer*); IBM's fonts were
  cloned by every BIOS vendor for decades without litigation.
  Well-founded **for the US**; some jurisdictions (Germany, UK)
  protect typefaces — essentially zero practical risk, small
  theoretical non-US risk.
- **Format:** TTF/OTB + PNG specimens + raw bitmaps in the extras;
  conversion easy.
- **Readability:** The CGA/EGA 8x8 faces are the gold standard for
  readable 8x8: distinct 1/l/I, O/0, decent pseudo-descenders.

#### 3. ZX Origins (Damien Guard) — https://damieng.com/typography/zx-origins/
- **License:** Informal: fonts are "freely available to be used in
  games you create in exchange for a mention in the credits section
  or perhaps a coffee." Commercial use explicitly allowed;
  recommended credit "*[fontname]* font by DamienG". **The one
  prohibited use: "redistributing the font as a font."**
- **The catch:** a chargen ROM in a public repo *is* redistributable
  font data — a raw 768-byte glyph table sits in a gray zone
  between "used in a product" (allowed) and "redistributed as a
  font" (not). He is explicitly open to email; one message would
  settle it. Not a drop-in for a strict license table without that.
- **Collection:** 263 original 8x8 typefaces, each shipped as TTF
  **plus C headers and Z80/6502/x86/68000 assembly** — already
  8-bytes-per-glyph. Coverage full printable ASCII (Spectrum
  heritage), typically no box drawing.
- **Standout readable faces:** **Envious** (very clean terminal
  face), **Localhost**, **Keytop**, **Clear Plan**, **Computer**.

#### 4. unscii-8 — http://viznut.fi/unscii/ (repo: https://github.com/viznut/unscii)
- **License:** "You can consider it Public Domain (or CC-0)" except
  the Unifont-derived files (unifont.hex, unscii-16-full) which are
  GPL. **unscii-8 is PD/CC0.**
- **Coverage:** Huge — best in this sweep. Full ASCII, Latin-1, box
  drawing, block elements, Teletext/Videotex mosaics, PETSCII
  pseudographics, shades, round corners. Variants: unscii-8 plus
  stylistic 8x8s (thin, alt, fantasy, mcr).
- **Format:** .hex (trivially parseable), plus BDF/PCF/TTF/OTF.
  Conversion to ROM data is a 10-line script.
- **Readability:** Designed as a *usable terminal font*, not just
  retro pastiche — good 0/O and 1/l/I distinction, consistent
  stroke weight; compressed descenders (8px cell limit).

#### 5. Spleen — https://github.com/fcambus/spleen
BSD 2-Clause. **Sizes: 5x8, 6x12, 8x16, 12x24, 16x32, 32x64 — no
8x8 exists.** Dismissed. (If an 8x16 is ever wanted for an
80-column mode, Spleen 8x16 with full CP437 + BSD-2 is a top pick.)

#### 6. Fantasy-console and homebrew fonts
- **TIC-80:** project MIT, but the system font is **6x6** in 8x8
  sprite cells. Dismissed on size.
- **PICO-8:** font and palette are **CC0** (official FAQ) — but
  glyphs are 3x5. Dismissed on size.
- **Pixel Operator** (Jayvee Enaguas) — **CC0 1.0**
  (fontlibrary.org; source notabug.org/HarvettFox96/ttf-pixeloperator).
  8px-height mono variants exist but ship **TTF only** — rasterize
  at 8px and verify the advance is actually 8. Usable with modest
  work; license perfect.
- **Kitchen Sink** (Polyducks, itch.io) — **6x8, not 8x8**, and
  "redistributing the font as an asset is prohibited" + an NFT
  clause. **Excluded** on both size and license.
- **Portfolio 6x8:** 6x8 (Atari Portfolio). Dismissed.

#### 7. Terminus
Sizes 6x12 through 16x32; **no 8x8**. SIL OFL 1.1. Dismissed.

#### 8. Linux consolefonts and other BDF/PSF sources
- **Kernel `lib/fonts/font_8x8.c`:** SPDX **GPL-2.0**, "generated
  by cpi2fnt," no origin credit. Same IBM-derived CP437 face as
  font8x8, but taking it from the kernel imports GPL-2.0 —
  pointless when dhepper/font8x8 offers equivalent glyphs as PD.
  Same for `font_pearl_8x8.c`. Skip.
- **IBM BIOS font recreations:** the canonical open ones are
  exactly the int10h pack (CC BY-SA) and dhepper/font8x8 (PD).
  Nothing cleaner-licensed found; nothing else notable at 8x8
  surfaced that beats the above.

#### Ranked top 3 (report 2)
1. **unscii-8** — PD/CC0, widest coverage by far (ideal raw
   material for a fantasy machine's full 256-glyph chargen), .hex
   converts trivially, genuinely readable. Cleanest license + best
   fit. Watch-out: don't grab the Unifont-derived files (irrelevant
   at 8x8).
2. **dhepper/font8x8** — already literally chargen-format C arrays,
   PD-labeled, ASCII+Latin+box+blocks. Slightly weaker provenance
   story but universally used; fine as fallback or "boring
   default."
3. **Ultimate Oldschool PC Font Pack (IBM CGA 8x8 / ATI 8x8 /
   Verite 8x8)** — the most authentic and most readable faces, but
   CC BY-SA 4.0 means attribution + share-alike on the converted
   glyph data — one viral-ish row in the license table. Use for the
   real CGA look if the flag is acceptable.

ZX Origins is the honorable mention: 263 original faces,
pre-converted 6502 source, but the "don't redistribute as a font"
clause needs one clarifying email before a raw glyph table lands in
a public repo.

---

### The combined ranking

For the K4510 chargen, both sweeps agree on the shape of the answer:

1. **open-roms `chargen_openroms.rom`** (LGPL-3.0) — the only
   ready-made, complete, 512-glyph PETSCII chargen in drop-in
   8-bytes-per-glyph format; zero conversion work. PXLfont 2.3 from
   the same directory (same license route) if a nicer face is
   wanted.
2. **unscii-8** (public domain) — the no-strings powerhouse for
   both ASCII and PETSCII repertoires; requires assembling the
   512-entry layout via the L2/19-025 PETSCII→Unicode mapping, and
   the look is its own.
3. **BESCII** (CC0) — the most Commodore-flavored clean design;
   TTF→bitmap extraction needed (lossless — it's a true 8x8 grid).

The prettiest candidates (Pet Me, Style64) are license-excluded;
pixel-identical recreations of the Commodore charset are avoided on
principle regardless of who typed them in. No decision taken yet —
this entry is the research record; the pick is Doc's.
