#!/bin/sh
# DELETE: the shell's safe remove -- move to /.TRASH, list it, restore from it,
# empty it.  And the property that matters more than any single case: DELETE
# and RANGER's DD must agree, because two trashes with different rules would be
# worse than no trash at all.
set -e
cd "$(dirname "$0")/.."
D=fs/ZDTEST
cleanup() { rm -rf "$D" fs/.TRASH; }
trap cleanup EXIT
cleanup
mkdir -p "$D"

R() { ./test/headless rom/kernal.bin "$1" "${2:-1400}" 2>/dev/null; }
fail() { echo "$out"; echo "deletetest: FAILED: $1"; exit 1; }
has()   { echo "$out" | grep -q -- "$1" || fail "$2"; }
hasnt() { if echo "$out" | grep -q -- "$1"; then fail "$2"; fi; }

# 1. it moves rather than destroys
printf 'one\n' > "$D/A.TXT"
printf 'two\n' > "$D/B.TXT"
out=$(R '~CD ZDTEST
~DELETE A.TXT B.TXT
~DIR
')
has "moved 2 items"     "DELETE did not report moving both"
has "0 file(s)"         "the files are still in the directory"
[ -f fs/.TRASH/A.TXT ] || fail "A.TXT is not in the trash"
grep -q one fs/.TRASH/A.TXT || fail "the trashed file lost its contents"

# 2. a name already in the trash must not be eaten
printf 'again\n' > "$D/A.TXT"
out=$(R '~CD ZDTEST
~DELETE A.TXT
~DELETE -l
')
[ -f 'fs/.TRASH/A.TXT~1' ]      || fail "the second A.TXT was not given a ~1 name"
grep -q one   fs/.TRASH/A.TXT   || fail "the FIRST A.TXT was overwritten"
grep -q again 'fs/.TRASH/A.TXT~1' || fail "the second A.TXT has the wrong contents"
has "3 items in /.TRASH"        "-l miscounted (DIR1 opens the directory, it is not an entry)"

# 3. restore brings it back, and will not clobber a name in the way
out=$(R '~CD ZDTEST
~DELETE -r A.TXT
~TYPE A.TXT
')
has "restored A.TXT" "-r did not restore"
has "one"            "-r restored the wrong file"
# B.TXT is still in the trash from step 1; put one of that name in the way.
printf 'in the way\n' > "$D/B.TXT"
out=$(R '~CD ZDTEST
~DELETE -r B.TXT
')
has "taken here"     "-r overwrote, or did not refuse, an existing name"
grep -q "in the way" "$D/B.TXT" || fail "-r OVERWROTE the file that was in the way"

# 4. empty really does delete
out=$(R '~CD ZDTEST
~DELETE -e
~DELETE -l
')
has "destroyed"          "-e reported nothing"
has "the trash is empty" "-l after -e should say the trash is empty"
[ -f 'fs/.TRASH/A.TXT~1' ] && fail "-e left files behind"

# 5. DELETE and RANGER's DD must use the same trash, with the same rules.
# The fixture is emptied first so SAME.TXT is the only entry -- otherwise the
# bar lands on whatever sorts first and DD trashes the wrong file, which is
# what the first version of this case actually did.
rm -f "$D"/*
printf 'from delete\n' > "$D/SAME.TXT"
out=$(R '~CD ZDTEST
~DELETE SAME.TXT
')
printf 'from ranger\n' > "$D/SAME.TXT"
out=$(R '~RANGER
~G~l~DD~y~q
' 1600)
[ -f fs/.TRASH/SAME.TXT ]      || fail "the two do not share /.TRASH"
[ -f 'fs/.TRASH/SAME.TXT~1' ]  || fail "RANGER did not apply the same ~1 rule as DELETE"
grep -q "from delete" fs/.TRASH/SAME.TXT      || fail "RANGER's DD overwrote what DELETE had put there"
grep -q "from ranger" 'fs/.TRASH/SAME.TXT~1'  || fail "RANGER's file went to the wrong name"

# 6. no arguments explains itself rather than doing something
out=$(R '~DELETE
')
has "not destroyed" "bare DELETE should say what it does"

echo "deletetest: OK (move, list, restore, empty; a taken name gets ~1; DELETE and RANGER's DD share one trash and one set of rules)"
