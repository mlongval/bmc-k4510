# data/fonts/zx — ZX Origins screen fonts (not committed)

A curated dozen of the [ZX Origins](https://damieng.com/zx-origins) 8×8
fonts, offered in the machine's **F7 → Video → Screen font** menu.

**Fonts by Damien Guard — ZX Origins, Copyright © 1988–2023 Damien Guard.**
https://damieng.com/zx-origins

## Why the `.bin` files are not in the repo

ZX Origins fonts are free to *use* (this machine is such a use), but the
licence forbids **re-hosting the files** — "redistributing this font as a
font … re-hosting the files on your own site or bundling it with other art
assets." This repository is public, so the `.bin` files are gitignored
(`data/fonts/zx/*.bin`) exactly the way Commodore's `chargen.bin` is: the
machine *names* them, it does not *ship* them. When a font is absent the
menu entry still shows but falls back to the kernel8 glyphs.

This is personal-use territory: on your own machines and your own SD card
the fonts are fine; the repo just must not carry them.

## Regenerating them

Download the fonts you want from https://damieng.com/zx-origins (each is a
`.zip`), then from the repo root:

    tools/mkzxfonts.py [folder-with-the-zips]

It reads each zip's `C64/<name>.bin` (a 4096-byte C64 character ROM),
**swaps its two 2048-byte charsets** — ZX Origins stores the lower/upper
set first, and the machine's `petscii_to_ascii()` (sdl/main.c) reads it
from the second half, the standard C64 order — and writes
`data/fonts/zx/<slug>.bin`. The swap is the whole trick: without it the
upper/graphics set lands where the loader expects lower/upper, and the text
comes out as line-drawing characters.

## The curated dozen

The menu order (and `FONT_ZX_*` in `core/ui/settings.h`):

| slug | menu name | ZX Origins font |
|---|---|---|
| bauhaus   | Bauhaus   | Bauhaus |
| broadway  | Broadway  | Broadwary |
| computer  | Computer  | Computer |
| cyberwire | Cyberwire | Cyberwire |
| nlq       | NLQ       | NLQ |
| benguiat  | Benguiat  | ZX Benguiat |
| chicago   | Chicago   | ZX Chicago |
| courier   | Courier   | ZX Courier |
| eurostile | Eurostile | ZX Eurostile |
| ocr-a     | OCR-A     | ZX OCR-A |
| pristine  | Pristine  | Pristine |
| anvil     | Anvil     | Anvil |

To change the set, edit `CURATED` in `tools/mkzxfonts.py` and the three
lists that must stay in step: `FONT_ZX_*` (settings.h), `font_names[]`
(settings.c), and `paths[]` in `apply_font` (sdl/main.c).
