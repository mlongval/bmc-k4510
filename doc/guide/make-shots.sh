#!/bin/sh
# Every screenshot in the guide is captured from the running machine, here.
# Needs test/capture built and rom/ + fs/ present (built or copied from the
# machine that builds them). A shot that cannot be produced fails the build.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../.." && pwd)
cd "$REPO"
shot() { # name frames keys
    test/capture rom/kernal.bin "$2" "$HERE/shots/$1.png" "$3" >/dev/null
    [ -s "$HERE/shots/$1.png" ] || { echo "shot $1 FAILED"; exit 1; }
}
shot boot 40 ""
shot dir 60 "dir
"
shot mon 80 "mon
e000.e00f
"
shot demos 900 "run ehbasic

RUN \"DEMOS.BAS\"
"
echo "shots: done"
