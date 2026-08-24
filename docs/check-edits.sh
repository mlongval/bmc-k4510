#!/bin/bash
# Show what changed since the last baseline snapshot.
#   ./check-edits.sh          show the diff
#   ./check-edits.sh accept   re-snapshot (after edits are folded in)
cd "$(dirname "$0")"
FILES="K4510-Design.md FEATURES.txt"

if [ "$1" = "accept" ]; then
  cp $FILES .baseline/ && echo "Baseline updated."
  exit 0
fi

changed=0
for f in $FILES; do
  if ! diff -q ".baseline/$f" "$f" >/dev/null 2>&1; then
    echo "=== $f ==="
    diff -u ".baseline/$f" "$f" | tail -n +3
    echo
    changed=1
  fi
done
[ $changed -eq 0 ] && echo "(no changes since baseline)"
exit 0
