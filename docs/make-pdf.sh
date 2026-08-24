#!/bin/bash
# Render the K4510 design documents into a single readable PDF.
set -e
cd "$(dirname "$0")"

cat > .title.md <<'HDR'
---
title: "BMC64-K4510"
subtitle: "A Modern C64 Successor for the Bare-Metal Raspberry Pi 3B+"
author: "Design Document"
date: "2026-08-21"
toc: true
toc-depth: 3
numbersections: true
geometry: "top=2.5cm,bottom=2.5cm,left=3cm,right=3cm"
fontsize: 11pt
linestretch: 1.15
mainfont: "DejaVu Serif"
sansfont: "DejaVu Sans"
monofont: "DejaVu Sans Mono"
monofontoptions: "Scale=0.82"
colorlinks: true
linkcolor: "black"
urlcolor: "blue"
HDR
echo '---' >> .title.md

pandoc .title.md K4510-Design.md \
  --pdf-engine=xelatex \
  --from=markdown+pipe_tables \
  -V documentclass=report \
  -o K4510-Design.pdf

rm -f .title.md
echo "Wrote K4510-Design.pdf"
