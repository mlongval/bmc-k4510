
(* file mode *)
	fmOpenRead	= $04;
	fmOpenWrite	= $08;
	fmOpenAppend	= $09;
	fmOpenReadWrite	= $0c;

var
	DateSeparator: Char = '-';

	FileMode: byte = fmOpenReadWrite;

	GraphMode: byte;		(* @var current screen mode *)

	IOResult: byte;			(* @var result of last file IO operation *)

	EoLn: Boolean;			(* @var end of line status *)

	ScreenWidth: smallint = 79;	(* @var current screen width (the ROM's MODE 1 1; JIM's COLS at $DA05 is the truth) *)
	ScreenHeight: smallint = 29;	(* @var current screen height *)
