#!/bin/sh
# Microsoft BASIC (basic/msbasic/ + basic/k4510msbasic.asm), driven from the
# shell exactly as a user would: RUN msbasic, then type at it.
#
# Unlike basictest.sh there is no self-checking .BAS to load -- this BASIC
# has no LOAD yet -- so the program is typed in and the answers are read off
# the screen.  Every check is something a port gets wrong: the cold-start
# prompts being answered from the canned input (a port that gets this wrong
# hangs at "MEMORY SIZE?"), the 9-digit floating point, the CR/LF pairing
# through k_chrout, upper-case folding of typed lower case, and Ctrl-C
# reaching ISCNTC through the keyboard queue's break flag at $D103.
cd "$(dirname "$0")/.."

fail() { echo "$out"; echo "msbasictest: FAILED: $1"; exit 1; }

# cold start, a loop, 9-digit FP, strings, and lower case typed at it
out=$(./test/headless rom/kernal.bin 'CD /MSBASIC
RUN msbasic
10 FOR I=1 TO 3
20 PRINT I;I*I;SQR(I)
30 NEXT
RUN
PRINT 355/113
A$="K4510"
print left$(a$,2);mid$(a$,2,3);len(a$)
' 3000 2>&1) || fail "MS BASIC did not run"

# MS BASIC never echoes what is typed -- INLIN reads through MONRDKEY and
# prints nothing back -- so the echo is the port's job.  Without it the
# machine looks like it is ignoring the keyboard, which is exactly how this
# was first reported.
echo "$out" | grep -q "PRINT 355/113"  || fail "typed command line not echoed (MONRDKEY echo)"
echo "$out" | grep -q "^OK"            || fail "no OK prompt"
echo "$out" | grep -q "3.14159292"     || fail "floating point division"
echo "$out" | grep -q "K4451 5"        || fail "strings, or lower case was not folded up"
# Three consecutive loop lines: if the LF of BASIC's CR/LF pair reached
# k_chrout (which makes a whole newline of CR *and* LF) the output would be
# double spaced.  headless prints only non-blank rows, so the blank lines
# themselves cannot be seen -- but at 60 rows the run would scroll the first
# iteration off the top before the last one printed.
for n in "1  1  1" "2  4  1.41421356" "3  9  1.73205081"; do
    echo "$out" | grep -q "$n" || fail "loop output line '$n' missing"
done

# The cold start, in a session short enough that the top of it is still on
# the 60-row screen.  Both canned answers are echoed, which is what makes
# them visible here at all.
out=$(./test/headless rom/kernal.bin 'CD /MSBASIC
RUN msbasic
PRINT 1
' 1200 2>&1) || fail "MS BASIC did not run (cold-start test)"
echo "$out" | grep -q "MEMORY SIZE? 28672" || fail "MEMORY SIZE? not answered or not echoed"
echo "$out" | grep -q "TERMINAL WIDTH? 80" || fail "TERMINAL WIDTH? not answered or not echoed"
echo "$out" | grep -q "BYTES FREE"         || fail "no cold-start banner"
echo "$out" | grep -q "COPYRIGHT 1977"     || fail "no Microsoft banner"

# Ctrl-C into a running program: the break flag at $D103, not a queue poll
out=$(./test/headless rom/kernal.bin 'CD /MSBASIC
RUN msbasic
10 GOTO 10
RUN
~~'"$(printf '\003')"'
' 3000 2>&1) || fail "MS BASIC did not run (break test)"
echo "$out" | grep -q "BREAK IN  10"   || fail "Ctrl-C did not break into line 10"

echo "msbasictest: OK (cold start answered, echo, FOR/NEXT, 9-digit FP, strings, case folding, Ctrl-C break)"
