# BMC-K4510

A fantasy 8/16-bit computer — 45GS02 CPU, 256 MB, its own video chip
(VICKe), four SIDs + OPL2 — running bare metal on a Raspberry Pi 3B+
via BMC64's Circle layer, and on the desktop via SDL2.

Design documents live in the sibling folder on ubuntu-s1
(`~/Projects/BMC64k4502/`). This repo is the machine.

    make test        # CPU wrapper test (no display)
    make             # everything
    ./sdl/k4510      # run it

## Layout

    core/xemu/   cpu65.c + cpu65.h from Xemu (GPL2), unchanged
    core/        shim header, memory system, VICKe
    sdl/         desktop frontend
    rom/         system ROM sources (ACME)
    test/        tests

## Provenance

`core/xemu/cpu65.c` is Gábor Lénárt's 65xx core from
https://github.com/lgblgblgb/xemu, taken byte-for-byte so upstream
fixes can be dropped in. `core/xemu/emutools_basicdefs.h` is ours and
is the entire environment it needs. reSID (Dag Lem) and fmopl (Jarek
Burczynski / Tatsuyuki Satoh) will be vendored the same way.
