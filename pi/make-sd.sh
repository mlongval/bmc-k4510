#!/bin/sh
# Lay out an SD card for the BMC-K4510: firmware + kernel8.img at the root,
# the machine's files under /k4510.  Usage: pi/make-sd.sh /path/to/mounted/card
set -e
M="$1"; [ -d "$M" ] || { echo "usage: $0 /mounted/card"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/.." && pwd)
B="${SHIM:-$HOME/Projects/k4510-pi/circle-libsdl2}/circle-stdlib-rpi3/libs/circle/boot"
cp "$B/bootcode.bin" "$B/start.elf" "$B/fixup.dat" "$M/"
cp "$HERE/config.txt" "$M/config.txt"
cp "$HERE/kernel8.img" "$M/kernel8.img"
mkdir -p "$M/k4510/rom" "$M/k4510/data" "$M/k4510/fs"
cp "$REPO/rom/kernal.bin" "$REPO/rom/wozmon.bin" "$REPO/rom/demo.bin" "$M/k4510/rom/"
cp "$REPO/data/font8.bin" "$M/k4510/data/"
cp "$REPO"/fs/* "$M/k4510/fs/"
sync
echo "card ready:"; ls -R "$M" | head -40
