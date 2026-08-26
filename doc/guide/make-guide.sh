#!/bin/sh
# Build the BMC-K4510 guide: screenshots first (from the machine itself),
# then XeLaTeX twice, then check the margins. Run from anywhere.
#
# Two things fail this build, both for the same reason -- a book you cannot
# trust is worse than no book:
#   * a screenshot that cannot be produced (make-shots.sh)
#   * a line that runs off the right margin (below)
# A5 is a narrow measure and monospace does not hyphenate, so overfull
# boxes happen. Set a path in prose with \pth{...} (breaks at / and .),
# give a wide table a p{} column at \small, and keep \verb short.
#
# OVERFULL_OK=1 ./make-guide.sh   builds anyway, for work in progress.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
"$HERE/make-shots.sh"
cd "$HERE"

# The cover carries a version and a build date. GUIDE_VERSION=... overrides
# the git description; the date is always today, written DD.MM.YYYY.
# Short enough to sit on the cover's first line: tag+commits, e.g. alpha-0.2+88.
if [ -z "$GUIDE_VERSION" ]; then
    TAG=$(git -C "$HERE" describe --tags --abbrev=0 2>/dev/null) || TAG=""
    if [ -n "$TAG" ]; then
        N=$(git -C "$HERE" rev-list "$TAG"..HEAD --count 2>/dev/null || echo 0)
        [ "$N" = 0 ] && GUIDE_VERSION="$TAG" || GUIDE_VERSION="$TAG+$N"
    else
        GUIDE_VERSION=unversioned
    fi
fi
VER=$GUIDE_VERSION
printf '%s\n%s\n' \
  "\\renewcommand{\\guideversion}{$VER}" \
  "\\renewcommand{\\guidedate}{$(date +%d.%m.%Y)}" > version.tex

xelatex -interaction=nonstopmode k4510-guide.tex >/dev/null
xelatex -interaction=nonstopmode k4510-guide.tex | tail -2

# Overfull boxes, with the page each one lands on. TeX prints page numbers
# as [N as it ships them, so counting those up to the complaint locates it.
awk '
  { n = split($0, part, /\[/)
    for (i = 2; i <= n; i++) if (part[i] ~ /^[0-9]/) { page = part[i] + 0 }
  }
  /^Overfull \\hbox/ {
    getline ctx
    gsub(/\\[A-Za-z]+\/[^ ]*/, "", ctx); gsub(/\[\]/, "", ctx)
    gsub(/^ +| +$/, "", ctx)
    amt = $3; gsub(/[()]/, "", amt)
    printf "  p.%-4d %-12s %.60s\n", page + 1, amt, ctx
    bad++
  }
  END { exit (bad > 0) }
' k4510-guide.log > "$HERE/.overfull" || {
    echo
    echo "MARGINS: $(wc -l < "$HERE/.overfull") line(s) run off the right margin:"
    cat "$HERE/.overfull"
    echo
    echo "Fix them (\\pth{} for paths, p{} + \\small for wide tables), or"
    echo "build with OVERFULL_OK=1 to accept them."
    [ -n "$OVERFULL_OK" ] || exit 1
}
rm -f "$HERE/.overfull"
echo "-> $HERE/k4510-guide.pdf"
