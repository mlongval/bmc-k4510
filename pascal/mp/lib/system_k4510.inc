(* [BMC-K4510] the SYSTEM unit's machine-specific half: frames from SYS
   ($D50D), keys from the keyboard device, arguments from the ROM's ARGS
   call ($FF95). *)

procedure Pause; assembler; overload;
(*
@description:
Delay program execution (one frame, 1/60 second).
*)
asm
	lda $D50D
w1	cmp $D50D
	beq w1
end;


procedure Pause(n: word); assembler; overload;
(*
@description:
Delay program execution (N frames).

@param: N - number of frames (1/60 second each)
*)
asm
loop	lda n
	ora n+1
	beq stop

	lda $D50D
w1	cmp $D50D
	beq w1

	dew n

	jmp loop
stop
end;


procedure Randomize; assembler;
(*
@description:
Initialize random number generator from the frame counter and the clock
*)
asm
	lda $D504		; latch the host clock
	lda $D50D
	eor $D505
	sta RndSeed
	lda $D50E
	eor $D506
	ora #$01
	sta RndSeed+1
end;


function Random: Real; overload;
(*
@description:
Generate random number

@returns: Real (Q24.8)
*)
begin

 RndSeed := $4595 * RndSeed + 1;

 asm
	lda RndSeed+1
	sta Result

	lda #$00
	sta Result+1
	sta Result+2
	sta Result+3
 end;

 Result:= 1 - Result;

end;


function RandomF: Float;
(*
@description:
Generate random number

@returns: Float (Single)
*)
begin

 RndSeed := $4595 * RndSeed + 1;

 asm
	lda RndSeed
	sta Result+1

	lda RndSeed+1
	and #$7f
	sta Result+2

	lda #$00
	sta Result
	lda #$3f
	sta Result+3
 end;

 Result:= 1 - Result;

end;


function RandomF16: Float16;
(*
@description:
Generate random number

@returns: Float16 (Half-Single)
*)
var a: word;
begin

 RndSeed := $4595 * RndSeed + 1;
 a := word(RndSeed) and $7fff;

 Result:= a * (1/32767);

end;


function Random(range: byte): byte; assembler; overload;
(*
@description:
Generate random number

@param: range (0..range-1)

@returns: byte
*)
asm
	lda RndSeed		; an 8-bit LFSR (taps $1D), stirred by the frame counter
	asl @
	bcc @+
	eor #$1D
@	eor $D50D
	sta RndSeed
	inc RndSeed+1
	eor RndSeed+1
	tay

	lda	range
	beq	stop

	sta	:ecx
	sty	:eax

	jsr	imulCL
	tay

stop	sty	Result
end;


function Random(range: smallint): smallint; overload;
(*
@description:
Generate random number

@param: range (0..range-1), (-range..0)

@returns: smallint
*)
begin

 if range = 0 then
	Result := 0
 else begin

	RndSeed := $4595 * RndSeed + 1;
	Result := RndSeed mod range;

asm
	lda range+1
	bpl plus

	lda Result+1
	bmi ok
	bpl sign

plus	lda Result+1
	bpl ok

sign	lda #0
	sub Result
	sta Result

	lda #0
	sbc Result+1
	sta Result+1
ok
end;

 end;

end;


function ParamCount: byte; assembler;
(*
@description:
Return number of command-line parameters passed to the program
(the words after the program's name on the shell line; the ROM's ARGS call).

@returns: byte
*)
asm
	txa:pha
	jsr k4_args		; A = the length of the tail, $F0/$F1 -> it
	sta :ztmp8
	ldx #0			; words
	ldy #0			; index
	stz :ztmp9		; 1 = inside a word
loop	cpy :ztmp8
	bcs done
	lda ($F0),y
	cmp #' '
	beq space
	lda :ztmp9
	bne next
	inx
	inc :ztmp9
	bne next
space	stz :ztmp9
next	iny
	bne loop
done	stx Result
	pla:tax
end;


function ParamStr(i: byte): TString; assembler;
(*
@description:
Return value of a command-line argument (1..ParamCount); 0 is the whole tail.

@param: i - index of the argument

@returns: string[32]
*)
asm
	txa:pha
	jsr k4_args
	sta :ztmp8		; n
	ldx #0			; the word we are at
	ldy #0			; index into the tail
	stz :ztmp9		; the result's length
	lda i
	bne skipsp
whole	cpy :ztmp8
	bcs done
	cpy #32
	bcs done
	lda ($F0),y
	sta adr.Result+1,y
	iny
	inc :ztmp9
	bne whole
skipsp	cpy :ztmp8
	bcs done
	lda ($F0),y
	cmp #' '
	bne word
	iny
	bne skipsp
word	inx
	cpx i
	beq copy
skipw	cpy :ztmp8
	bcs done
	lda ($F0),y
	cmp #' '
	beq skipsp
	iny
	bne skipw
copy	cpy :ztmp8
	bcs done
	lda ($F0),y
	cmp #' '
	beq done
	ldx :ztmp9
	cpx #32
	bcs done
	sta adr.Result+1,x
	inc :ztmp9
	iny
	bne copy
done	lda :ztmp9
	sta adr.Result
	pla:tax
end;
