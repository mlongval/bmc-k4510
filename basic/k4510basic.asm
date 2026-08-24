; EhBASIC 2.22 on the BMC-K4510, as a .prg the system ROM loads at $7000.
; "Derived from EhBASIC" -- Lee Davison's Enhanced BASIC, see README-EhBASIC.txt.
; Console through the ROM jump table ($FF80 CHROUT, $FF86 GETIN); BASIC RAM
; $0800-$6FFF (26 KB); page 3 holds EhBASIC's vectors and input buffer;
; the whole zero page is EhBASIC's (the ROM saves its own around calls).
; RUN/STOP (Escape) resets the machine back to the shell.

IRQ_vec	= VEC_SV+2		; EhBASIC keeps its page-3 layout (Ibuffs follows)
NMI_vec	= IRQ_vec+$0A
k_crx0	= $03B3			; K4510: the crunch start index (the * prefix is only a prefix there)

K4510_TAIL = $BA00			; = Ram_top: BASIC's RAM ends where the interpreter tail begins

; K4SG header, stage 3 of the memory plan: the interpreter loads in three
; segments above BASIC's RAM -- $E000-$FEFF (block 7) + $C000-$CFFF
; (block 6) + a tail at $BC00 in the RAM under the sideways window. The
; loader sets the bank bases; the launch trampoline engages blocks 5-7.
; BASIC's program RAM is $0800-$BBFF: 46079 bytes free.
	.byte	"K4SG"
	.byte	3, 0			; segments, flags
	.word	k4510_start		; entry
	.dword	$E000
	.dword	K4510_SPLIT1 - $E000
	.byte	7, 0, 0, 0
	.dword	$C000
	.dword	K4510_SPLIT2 - $C000
	.byte	6, 0, 0, 0
	.dword	K4510_TAIL
	.dword	K4510_END - K4510_TAIL
	.byte	$FF, 0, 0, 0		; no bank: lives in the RAM under the window

	.include "basic.asm"		; .org $E000 / $C000 inside; Ram_base/Ram_top patched for the K4510

; ---- the K4510 host glue -------------------------------------------------

ROM_CHROUT	= $FF80
ROM_GETIN	= $FF86
CR		= $0D
LF		= $0A
ESC		= $1B

k4510_start
	CLD
	STZ	$03B2			; the sprite table wants clearing again (gsprinit)
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
; is taken (from anywhere in the queue), anything else stays for GET/INPUT.
k4510_cc
	LDA	ccflag
	BNE	k4510_cc_done		; checks inhibited (LOAD feeding a file)
	LDA	$D103			; a Ctrl-C or RUN/STOP anywhere in the queue? (removed; other keys stay for GET)
	BEQ	k4510_cc_done
	JSR	k4510_hush		; both stop the program and silence the SIDs; @BYE leaves to the shell
	LDA	#$03
	STA	ccbyte
	LDX	#$20
	STX	ccnull
	JMP	LAB_1636		; Ctrl-C: STOP
k4510_cc_done
	JMP	LAB_FBA2		; the interrupt checks, as in the stock routine

; silence the four SIDs: volume 0, every gate off
k4510_hush
	PHA
	PHX
	LDX	#0
k4510_hush1
	STZ	$D400,X			; registers 0-24 of chip 0; the other chips are 32 bytes apart
	STZ	$D420,X
	STZ	$D440,X
	STZ	$D460,X
	INX
	CPX	#25
	BNE	k4510_hush1
	PLX
	PLA
	RTS

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

	.include "k4510math.asm"
	.include "k4510expr.asm"

K4510_SPLIT2				; [BMC-K4510] the glue tail, in the RAM under the sideways window
	.assert K4510_SPLIT2 <= $D000, error, "EhBASIC $C000 slice overflows into the I/O page"
	.org	K4510_TAIL

	.include "k4510file.asm"
	.include "k4510gfx.asm"

k4510_banner
	.byte	CR, "BMC-K4510  EhBASIC 2.22 +GRAPHICS SPRITES PLOT LINE TRI  (RUN/STOP: shell)", CR, 0
K4510_END
	.assert K4510_END <= $C000, error, "EhBASIC tail overflows its slice"
