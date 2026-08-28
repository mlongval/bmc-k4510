#!/bin/sh
# Build fs/PRG/supermon.prg from mon/supermon.asm.
# Self-contained on purpose: needs 64tass (not cc65), so it is not part of
# the main Makefile -- the built .prg is committed, like every other .prg.
set -e
cd "$(dirname "$0")/.."
TASS=${TASS:-$HOME/.local/bin/64tass}
"$TASS" --m65c02 --nostart -o mon/supermon.raw mon/supermon.asm
# the ROM's .prg header: load address, run address (both $B000)
printf '\000\260\000\260' > fs/PRG/supermon.prg
cat mon/supermon.raw >> fs/PRG/supermon.prg
rm -f mon/supermon.raw
ls -l fs/PRG/supermon.prg
