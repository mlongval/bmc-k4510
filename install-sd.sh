#!/bin/sh
# BMC-K4510: put a release zip onto an SD card for the Raspberry Pi 3B+.
#
#   ./install-sd.sh bmc-k4510-pi3.zip /media/you/CARD      unzip onto a mounted FAT32 card
#   ./install-sd.sh bmc-k4510-pi3.zip /dev/sdX --format    wipe that device, make it FAT32, install
#
# Card choice matters: use an SDHC card (4-32 GB), specifically.
#  - SDHC comes FAT32 from the factory and just works.
#  - Old plain SD cards (2 GB and under) do NOT boot -- field-tested.
#  - SDXC (64 GB+) comes exFAT and will not boot until reformatted FAT32;
#    that is what --format is for.
# The machine needs about 5 MB, so the smallest SDHC card you can buy is plenty.
set -e

ZIP=$1; TARGET=$2; MODE=$3
usage() { echo "usage: $0 <release.zip> <mounted-card-path>"; echo "       $0 <release.zip> /dev/DEVICE --format   (DESTROYS the device)"; exit 1; }
[ -n "$ZIP" ] && [ -n "$TARGET" ] || usage
[ -f "$ZIP" ] || { echo "install-sd: $ZIP: no such file"; exit 1; }
unzip -l "$ZIP" | grep -q kernel8.img || { echo "install-sd: $ZIP does not look like a BMC-K4510 release zip (no kernel8.img)"; exit 1; }

if [ "$MODE" = "--format" ]; then
    DEV=$TARGET
    [ -b "$DEV" ] || { echo "install-sd: $DEV is not a block device"; exit 1; }
    case "$DEV" in *[0-9]) echo "install-sd: give the whole device (e.g. /dev/sdc), not a partition"; exit 1;; esac
    ROOTDEV=$(findmnt -no SOURCE / | sed 's/[0-9]*$//;s/p$//')
    [ "$DEV" = "$ROOTDEV" ] && { echo "install-sd: $DEV is the disk this system runs from. No."; exit 1; }
    echo "This will ERASE EVERYTHING on $DEV:"
    lsblk -o NAME,SIZE,MODEL,MOUNTPOINTS "$DEV"
    printf 'Type the device name (%s) to continue: ' "$(basename "$DEV")"
    read ANSWER
    [ "$ANSWER" = "$(basename "$DEV")" ] || { echo "install-sd: aborted"; exit 1; }
    for p in "$DEV"?*; do [ -b "$p" ] && sudo umount "$p" 2>/dev/null || true; done
    sudo wipefs -aq "$DEV"
    echo ',,0c,*' | sudo sfdisk -q "$DEV"
    sudo partprobe "$DEV"; sleep 1
    PART=$(lsblk -nrpo NAME "$DEV" | sed -n 2p)
    sudo mkfs.vfat -F 32 -n K4510 "$PART" >/dev/null
    MNT=$(mktemp -d)
    sudo mount "$PART" "$MNT"
    sudo unzip -oq "$ZIP" -d "$MNT"
    sync
    sudo umount "$MNT"; rmdir "$MNT"
    echo "install-sd: done. $DEV is a BMC-K4510 boot card -- put it in the Pi."
else
    [ -d "$TARGET" ] || { echo "install-sd: $TARGET is not a directory (mount the card first)"; exit 1; }
    FSTYPE=$(findmnt -no FSTYPE --target "$TARGET" 2>/dev/null || echo unknown)
    case "$FSTYPE" in
      vfat|fat32|msdos) : ;;
      exfat) echo "install-sd: $TARGET is exFAT -- the Pi will not boot it."
             echo "That is an SDXC card as shipped; rerun with /dev/DEVICE --format to make it FAT32."; exit 1 ;;
      *) echo "install-sd: warning: $TARGET is '$FSTYPE', the Pi needs FAT32; continuing anyway" ;;
    esac
    unzip -oq "$ZIP" -d "$TARGET"
    sync
    echo "install-sd: done. Eject the card and put it in the Pi."
fi
