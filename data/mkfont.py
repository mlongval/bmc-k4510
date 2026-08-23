# Build data/font8.bin: 256 glyphs x 8 rows, MSB-first, ASCII order, from the
# Linux kernel's 8x8 console font (lib/fonts/font_8x8.c, GPL-2.0).  Positions
# $20-$7E are ASCII; the rest is code page 437 (box drawing, accents).
#   python3 data/mkfont.py font_8x8.c data/font8.bin
import re, sys
src = open(sys.argv[1]).read()
body = src[src.index('FONTDATAMAX, 0 }, {') + 19:]          # after the header struct
vals = [int(v, 16) for v in re.findall(r'^\s*0x([0-9a-fA-F]{2}),', body, re.M)][:2048]
assert len(vals) == 2048, len(vals)
font = bytearray(vals)
font[0x00*8:0x00*8+8] = bytes(8)                      # glyph 0 blank (the text layer's "nothing")
open(sys.argv[2], 'wb').write(font)
print("font8.bin written from font_8x8.c (GPL-2.0)")
