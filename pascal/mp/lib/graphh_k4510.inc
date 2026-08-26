(* [BMC-K4510] GRAPH on VICKY: layer 1, a 640x480 8-bit bitmap at $200000
   (the same one BBC BASIC's ULA draws on), pixels through the 45GS02's
   flat store, lines/rectangles/triangles through the blitter. Pixel
   value 0 is transparent -- the console's text shows through -- so
   the ROM console and the picture coexist, as in a BBC MODE. Colours
   are the machine's palette (0-15 the C64 set). *)

const
	GFX_BASE = $200000;
	GFX_W = 640;
	GFX_H = 480;

var	WIN_LEFT: smallint = 0;
	WIN_RIGHT: smallint = 79;
	WIN_TOP: smallint = 0;
	WIN_BOTTOM: smallint = 29;

	procedure DrawLine(x0, y0, x1, y1: smallint);
	(* @description: a line by the blitter, in the current colour (Line and LineTo use it) *)
	procedure FillBar(x0, y0, x1, y1: smallint);
	(* @description: a filled rectangle by the blitter, corners in any order *)
	procedure FillTriangle(x0, y0, x1, y1, x2, y2: smallint);
	(* @description: a filled triangle by the blitter *)
	procedure ClearDevice;
	(* @description: the bitmap cleared to transparent (the text shows) *)
