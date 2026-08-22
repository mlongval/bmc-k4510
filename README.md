# BMC-K4510

A fantasy 8/16-bit computer — 45GS02 CPU, 256 MB, its own video chip
(VICKe), four SIDs + OPL2 — running bare metal on a Raspberry Pi 3B+
via BMC64's Circle layer, and on the desktop via SDL2.

This repo is the machine. The design documents (capability matrix,
plan, VICKe spec, build log) live in a separate folder on the
author's server and are not part of this repo.

    make test        # CPU wrapper test (no display)
    make             # everything
    ./sdl/k4510      # run it

## Layout

    core/xemu/   cpu65.c + cpu65.h from Xemu (GPL2), unchanged
    core/        shim header, memory system, VICKe
    sdl/         desktop frontend
    rom/         system ROM sources (ACME)
    test/        tests

## Targets

- Desktop: SDL2, Linux. `make && ./sdl/k4510`.
- Raspberry Pi 3B+ bare metal: the same frontend built against
  [circle-libsdl2](https://github.com/Xalior/circle-libsdl2) (planned,
  Phase 6).

## Provenance

`core/xemu/cpu65.c` is Gábor Lénárt's 65xx core from
https://github.com/lgblgblgb/xemu, taken byte-for-byte so upstream
fixes can be dropped in. `core/xemu/emutools_basicdefs.h` is ours and
is the entire environment it needs. reSID (Dag Lem) and fmopl (Jarek
Burczynski / Tatsuyuki Satoh) will be vendored the same way.
