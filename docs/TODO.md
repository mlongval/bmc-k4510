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
- [ ] **ROM2/ROM1C headroom plan** — if resident space is ever needed:
      move do_load (~1.2K) to SWCODE0, dump/poke/peek with mon_line,
      cmd_exec to SWCODE0; decide whether Wozmon retires in favour of
      SUPERMON.prg (~1.1K of ROM1A back).
- [ ] **User banks** — document the convention: sideways banks 3-15 are
      user RAM banks; the ROM never claims above bank 2.  (msbasic is
      the penciled first tenant of bank 3.)

## Strays

- [ ] `EDITTMP.BAS` in the machine filesystem root (and Doc's scratch
      files on the laptop: `fs/BBCBASIC/TEST.BBC.laptop-draft`,
      `fs/EHBASIC/EDITTMP.BAS`) — Doc to keep or delete.
- [ ] `fs/SYSTEM/PERF.TXT` — generated; add to .gitignore.
