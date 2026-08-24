# data/fonts — licence-clean fonts, vendored 2026-08-24

Staged 2026-08-24 by the archive-side session, from the font research
recorded in BUILD-LOG.md entry "(archive-side, 2)" (archive commit
9ea9086; living log entry (x)). Vendored from the archive staging area (see the BUILD-LOG).

## Contents

### openroms/ — LGPL-3.0-or-later
Fetched from github.com/MEGA65/open-roms (master, 2026-08-24):
- `chargen_openroms.rom` — 4096 bytes, 512 glyphs, **both PETSCII
  charsets**, already in 8-bytes-per-glyph chargen format. Drop-in.
- `chargen_pxlfont_2.3.rom` — same format, PXLfont 88665b RF2.3 by
  Retrofan; README.md here carries open-roms' permission statement
  (LGPL-3.0 *via open-roms only* — PXLfont's own terms elsewhere are
  permission-required).
- `8x8font.png` — the editable source of chargen_openroms.rom
  (ship it alongside: satisfies LGPL "source" for the data).
- `LICENSE` — LGPL-3.0 notice; copyright Paul Gardner-Stephen 2019,
  Roman Standzikowski (FeralChild64) 2019-2021.

### unscii/ — Public Domain
- `unscii-8.hex` — unscii 2.x by Viznut (viznut.fi/unscii/, fetched
  2026-08-24; note the site's TLS cert is expired — content intact).
  3191 glyphs incl. 213/214 of Unicode Symbols for Legacy Computing
  (the PETSCII block). PD explicitly ("the other variants are in the
  Public Domain" — only unscii-16-full is GPL, not this file).
- `font8-unscii.bin` — **generated here** by tools/hex2chargen.py: a
  drop-in replacement for `data/font8.bin` (256 glyphs × 8 bytes,
  MSB-first, CP437 layout, glyph 0 blank — same contract as
  data/mkfont.py). All 255 glyphs filled (⌐ synthesized as mirrored
  ¬, ∙ borrowed from middle dot). **Swapping this in retires the
  GPL-2.0 kernel-font row in LICENSES.md for a public-domain one.**

### bescii/ — CC0 1.0
Clone of codeberg.org/Dmian/font-bescii @ c9e53ad (git metadata stripped; only fonts/v3 Mono + .glyphs + LICENCE kept here — the full clone is in the archive staging). Damian Vila's PETSCII-inspired 8x8 redesign (deliberately
NOT pixel-identical to the Commodore ROM). `fonts/v3/` has
Bescii-Mono.ttf + bescii-v3.glyphs; LICENCE is full CC0 text.
Conversion to bitmap requires 8px rasterization (lossless — true 8x8
grid design) — not done yet; staged for when/if its look is chosen.

### tools/
- `hex2chargen.py` — .hex → font8.bin-format converter (see above).
  Can also drive a future PETSCII 512-glyph build from unscii using
  the Unicode L2/19-025 PETSCII→Unicode mapping tables.

## What remains (coding-session territory)
1. Vendor into the repo (suggested: `data/fonts/` or beside
   `data/font8.bin`), with these license notes.
2. LICENSES.md: add rows — open-roms chargen (LGPL-3.0-or-later, ship
   8x8font.png as source), PXLfont (LGPL-3.0 via open-roms), unscii-8
   (PD), BESCII (CC0 — only if used); swap the kernel-font row for
   font8-unscii.bin if adopted. **Also noticed missing from
   LICENSES.md: the `tube/` BBCTTY vendoring (zlib, R.T. Russell).**
3. Decide the text-mode font (font8-unscii.bin is the zero-work
   license upgrade) and the PETSCII story (openroms ROM is drop-in;
   VICKe wiring + any charset-switch plumbing is new work).
4. CREDITS.md draft is staged next to this file — verify and vendor.
