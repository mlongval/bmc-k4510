#!/bin/sh
# The two BASICs, tested from inside: fs/EHBASIC/TEST.BAS and
# fs/BBCBASIC/TEST.BBC are self-checking programs (arithmetic, the MATH
# unit's functions, strings, control flow, arrays, the machine's
# registers, graphics and sound escapes, files, the * escape); each
# prints a verdict line the harness reads off the screen.
cd "$(dirname "$0")/.."
rm -f fs/TESTOUT.BAS fs/TESTOUT.TXT
out=$(./test/headless rom/kernal.bin "RUN EHBASIC
~~~RUN \"TEST.BAS\"
~~~~~~~~~~~~~~~~~~~~" 1500 2>&1) || { echo "$out"; echo "basictest: FAILED: EhBASIC did not run"; exit 1; }
echo "$out" | grep -q "EHTEST PASSED" || { echo "$out"; echo "basictest: FAILED: EhBASIC"; exit 1; }
echo "$out" | grep -q "STAR OK" || { echo "$out"; echo "basictest: FAILED: EhBASIC * escape"; exit 1; }
[ -s fs/TESTOUT.BAS ] || { echo "basictest: FAILED: EhBASIC SAVE wrote nothing"; exit 1; }
grep -q "EHBASIC SELF-TEST" fs/TESTOUT.BAS || { echo "basictest: FAILED: EhBASIC SAVE content"; exit 1; }
rm -f fs/TESTOUT.BAS
TUBE=./test/tubetest; [ -x $TUBE ] || TUBE=./test/headless   # the in-process Tube when built (make tubetest), else the desktop one (tube/bbcbasic on a pty)
out=$($TUBE rom/kernal.bin "BBC
~~~LOAD \"BBCBASIC/TEST.BBC\"
~~RUN
~~~~~~~~~~~~~~~~~~~~~~~~*QUIT
~~" 2400 2>&1) || { echo "$out"; echo "basictest: FAILED: BBC BASIC did not run"; exit 1; }
echo "$out" | grep -q "BBCTEST PASSED" || { echo "$out"; echo "basictest: FAILED: BBC BASIC"; exit 1; }
echo "$out" | grep -q "STAR OK" || { echo "$out"; echo "basictest: FAILED: BBC BASIC * escape"; exit 1; }
rm -f fs/TESTOUT.TXT
echo "basictest: OK (EhBASIC 32 checks incl. MATH unit, graphics, SAVE and *; BBC BASIC 28 checks incl. files through the Tube, ULA graphics/sound/sprites and *)"
