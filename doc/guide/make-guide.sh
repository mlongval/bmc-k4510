#!/bin/sh
# Build the BMC-K4510 guide: screenshots first (from the machine itself),
# then XeLaTeX twice. Run from anywhere.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
"$HERE/make-shots.sh"
cd "$HERE"
xelatex -interaction=nonstopmode k4510-guide.tex >/dev/null
xelatex -interaction=nonstopmode k4510-guide.tex | tail -2
echo "-> $HERE/k4510-guide.pdf"
