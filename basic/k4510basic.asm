; EhBASIC 2.22 on the BMC-K4510, as a .prg the system ROM loads at $7000.
; "Derived from EhBASIC" -- Lee Davison's Enhanced BASIC, see README-EhBASIC.txt.
; Console through the ROM jump table ($FF80 CHROUT, $FF86 GETIN); BASIC RAM
; $0800-$6FFF (26 KB); page 3 holds EhBASIC's vectors and input buffer;
; the whole zero page is EhBASIC's (the ROM saves its own around calls).
; RUN/STOP (Escape) resets the machine back to the shell.

IRQ_vec	= VEC_SV+2		; EhBASIC keeps its page-3 layout (Ibuffs follows)
NMI_vec	= IRQ_vec+$0A

	.org	$6BFC
	.word	$6C00			; .prg header: load address
	.word	k4510_start		;              run address

	.include "basic.asm"		; .org $7000 inside, Ram_base/Ram_top patched for the K4510

; ---- the K4510 host glue -------------------------------------------------

ROM_CHROUT	= $FF80
ROM_GETIN	= $FF86
CR		= $0D
LF		= $0A
ESC		= $1B

k4510_start
	CLD
	LDA	#<k4510_in
	STA	VEC_IN
	LDA	#>k4510_in
	STA	VEC_IN+1
	LDA	#<k4510_out
	STA	VEC_OUT
	LDA	#>k4510_out
	STA	VEC_OUT+1
	LDA	#<k4510_load
	STA	VEC_LD
	LDA	#>k4510_load
	STA	VEC_LD+1
	LDA	#<k4510_save
	STA	VEC_SV
	LDA	#>k4510_save
	STA	VEC_SV+1
	LDY	#0
k4510_msg
	LDA	k4510_banner,Y
	BEQ	k4510_go
	JSR	k4510_out
	INY
	BNE	k4510_msg
k4510_go
	JMP	LAB_COLD

; non-halting input: A = char, carry set if one was there
k4510_in
	PHX
	PHY
	JSR	ROM_GETIN
	PLY
	PLX
	CMP	#0
	BEQ	k4510_nokey
	CMP	#ESC
	BEQ	k4510_quit
	CMP	#$61			; fold a-z to upper case: EhBASIC keywords are upper case
	BCC	k4510_gotkey
	CMP	#$7B
	BCS	k4510_gotkey
	AND	#$DF
k4510_gotkey
	SEC
	RTS
k4510_nokey
	CLC
	RTS
k4510_quit
	JMP	($FFFC)			; back to the shell, cold

; Ctrl-C check, called once per statement. EhBASIC's own version pops the
; input and keeps the byte for 32 statements, so a key typed while a program
; is busy is usually lost. This one only peeks ($D102): a Ctrl-C or RUN/STOP
; is taken, anything else stays in the FIFO for the next GET/INPUT.
k4510_cc
	LDA	ccflag
	BNE	k4510_cc_done		; checks inhibited (LOAD feeding a file)
	LDA	$D102			; next key, not popped
	BEQ	k4510_cc_done
	CMP	#$03
	BEQ	k4510_cc_brk
	CMP	#ESC
	BNE	k4510_cc_done
k4510_cc_brk
	JSR	k4510_in		; pop it (ESC resets into the shell from there)
	STA	ccbyte
	LDX	#$20
	STX	ccnull
	JMP	LAB_1636		; Ctrl-C: STOP
k4510_cc_done
	JMP	LAB_FBA2		; the interrupt checks, as in the stock routine

; output A; EhBASIC sends CR LF and the ROM's CHROUT makes a newline of either
k4510_out
	CMP	#LF
	BEQ	k4510_outdone
	PHA
	PHX
	PHY
	JSR	ROM_CHROUT
	PLY
	PLX
	PLA
k4510_outdone
	RTS

	.include "k4510file.asm"
	.include "k4510math.asm"
	.include "k4510expr.asm"

	.include "k4510gfx.asm"

k4510_banner
	.byte	CR, "BMC-K4510  EhBASIC 2.22 +GRAPHICS PLOT LINE TRI PALETTE GCLS  (RUN/STOP: shell)", CR, 0
