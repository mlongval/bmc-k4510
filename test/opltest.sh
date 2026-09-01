#!/bin/sh
# The OPL2 programs.  Both draw through the sprite engine and their own text
# layer at $123000, not the console's text32 at $30000 -- which is the only
# thing test/headless can read -- so what is checked here is that they run and
# hand the machine back, not what they put on the glass.  The picture is
# checked by eye against a screenshot; docs/notes/coding.md says so.
cd "$(dirname "$0")/.."

fail() { echo "$out"; echo "opltest: FAILED: $1"; exit 1; }

# OPLPLAY: runs, three tunes, and Q returns to a working shell.
out=$(./test/headless rom/kernal.bin 'OPLPLAY
~~~~ Q
ECHO OPLBACK
' 2400 2>&1) || fail "OPLPLAY did not run"
echo "$out" | grep -q "OPLBACK" || fail "OPLPLAY did not return to the shell"

# and the older nine-voice demo still does too
out=$(./test/headless rom/kernal.bin 'OPL2
~~~Q
ECHO OPL2BACK
' 2400 2>&1) || fail "OPL2 did not run"
echo "$out" | grep -q "OPL2BACK" || fail "OPL2 did not return to the shell"

echo "opltest: OK (OPLPLAY and OPL2 both run and hand the machine back)"
