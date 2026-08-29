# TODO — the next-week list (from the 2026-08-28 state review)

Raised in the organization review with Doc, 2026-08-28. Address, then
strike through or delete; when the file is empty, delete it.

## Operations

- [ ] **docs/HOSTS.md** — one row per machine: checkout path, toolchain
      location, the PATH line, the build command.  Ends the per-host
      archaeology (p15's checkout is `~/Projects/k4510-pi/k4510`, NOT the
      standard path; hdieu needs `~/opt/cc65/bin` on PATH; p15 must use
      the 15.2 ARM toolchain ONLY — mixing with 14.2 breaks arm_neon.h).
- [ ] **Move operational truth into the repo** — ROM budget rules
      (romfree.py first; segment map), the machine-sync recipes, the
      settings-save-on-exit behaviour, the two-agent tree conventions.
      Today much of this lives only in Claude session memory.
- [ ] **Pi verification** — write a card and run everything since
      alpha-0.3-105 on real hardware: the games, SUPERMON, KOMMANDER,
      the video-mode rebuild.  The desktop is verified; the Pi is not.

## Code debt

- [ ] **core/io.c split** into per-chip files (agreed earlier; unblocked).
- [ ] **Zero page relief (not urgent, fail-loud):** ROM ZP slice $02-$21
      is 32/32.  When convenient, widen into $22-$3F (grow crt0's zp_rom
      save buffer to match) or evict a non-hot crt0 zp var to BSS.
- [ ] **BSS relief (nearer):** BSSR $0440-$05FF is at 447/448.  Rebalance
      against the C stack above it, or audit for evictable statics.
- [x] **ROM2/ROM1C headroom plan** — ~~done 2026-08-29~~ (coding session;
      see `docs/notes/coding.md`).  `do_load` went to SWCODE0,
      `video_init`/`page_break`/`mode_do` from CODE to CODE2, and
      `cmd_save`/`cmd_type` to bank 1 through `sw_call`.  ROM1C 35 -> 646
      free, ROM2 30 -> 547, ROM1A 1110, SW1 973.  `peek` had to stay
      resident (INFO calls it from bank 1), so it and `dump`/`poke` did
      not move.  **Wozmon stays** (Doc, 2026-08-29): the 1.1K is the
      in-shell `MON`/`WOZ` command, and its whole value is being there
      when the machine is too broken to load `SUPERMON.prg` off disk.
      A boot-swapped monitor is **not** a substitute — the reset keeps
      RAM but loses the registers, zero page and the sideways banks (see
      `docs/BUILD-LOG.md`, 2026-08-29).
- [ ] **User banks** — document the convention: sideways banks 3-15 are
      user RAM banks; the ROM never claims above bank 2.  (msbasic is
      the penciled first tenant of bank 3.)
- [ ] **Parked — a SUPERMON kernal** (Doc's idea, 2026-08-29).  A
      boot-selectable monitor image, for when the kernal will not reach a
      prompt: SUPERMON rather than Wozmon, because resident `MON` already
      covers "always there" and a boot image should bring the good tools
      (full 45GS02 disassembly, assembler, hunt, transfer, compare).
      Cost is the console shim — `mon/supermon.asm` calls the ROM jump
      table ($FF80/$FF86/$FF89/$FF8C/$FF8F) and as a boot ROM has no K:OS
      under it, so it needs its own VICKY text init, key poll and reset
      vector; `rom/wozmon.a` is the worked example.  `L`/`S` need a
      decision (a monitor that cannot save what it recovered is half a
      tool).  Host side is ~10 lines: `ACT_POWER_CYCLE` without the
      `host_zero`, plus a setting for which image — a menu action, not a
      held key.  Reasoning in `docs/BUILD-LOG.md`, 2026-08-29.

## Naming — done 2026-08-29, shipped as alpha-0.4 'Imprint'

- [x] ~~**The guest strings**~~ — the ROM banner, the status bar and `INFO`
      say `BMC-K4510` from a ROM whose same bytes boot on both hosts.
      Three strings to `K4510` (`rom/kernal.c` 123, 765, 1526).  Changes
      three handbook figures, so coordinate with the handbook session
      before the next capture.
- [x] ~~**Shared host chrome**~~ — SDL window title, F7 menu heading,
      settings-file header, `core/io.c`'s dump header: `K4510`.
- [x] ~~**`README.md`'s opening**~~ — lead with the machine; the bare-metal
      Pi is one of the two ways to run it, not the definition.
- [x] ~~**File-header comments**~~ across `demo/`, `pascal/`, `basic/`,
      `forth/`, `tube/`, `cpm/`, `mon/`, `test/`, `tools/` — the bulk of
      the ~150 occurrences, and the least urgent.  `pi/`'s seven stay.
- [x] ~~**The handbook's title**~~ — now *The K4510 User's and
      Programmer's Guide*, cover reads `K4510`, new §1.3 "Two names, one
      machine", the thanks page names Randy Rossi in the appliance's
      name, and every figure was recaptured (the banner is in a dozen).
      Done by the coding session because Doc asked for the whole job in
      one pass — handbook session, it is yours to revise.

## Strays

- [ ] `EDITTMP.BAS` in the machine filesystem root (and Doc's scratch
      files on the laptop: `fs/BBCBASIC/TEST.BBC.laptop-draft`,
      `fs/EHBASIC/EDITTMP.BAS`) — Doc to keep or delete.
- [ ] `fs/SYSTEM/PERF.TXT` — generated; add to .gitignore.
