; [BMC-K4510] SINGLE (IEEE-754 32-bit) on the MATH unit at $D700 -- the
; same entry points and registers as common\single.asm (David Schmenk's
; software library), but every operation is a few register moves and one
; write to FOP: the unit does the arithmetic in the cycle after.
;
;   @FADD @FSUB @FMUL @FDIV   A = FP1MAN (eax), B = FP2MAN (edx) -> FPMAN (ecx)
;   @FCMPL                    A = FP1MAN, B = FPMAN -> A = sign(B - A): 0 equal, 1 B greater, $FF B less
;   @F2I  @I2F  @FFRAC        FPMAN -> FPMAN
;   @FROUND                   FP2MAN -> FPMAN (an integer: round half away from zero)
;   @NEGINT                   FPMAN -> -FPMAN (kept for the printer's sake)
;
; MATH unit: F0..F7 at $D700 + 4n; FOP $D720 (write = execute, low 5 bits
; the op); FARG $D721 = (dst << 4) | src; FFLAGS $D722 bit0 zero, bit1
; negative (from CMP: Fd - Fs); FI $D724 int32.

M_F0	= $D700
M_F1	= $D704
M_FOP	= $D720
M_FARG	= $D721
M_FLAGS	= $D722
M_FI	= $D724

M_ADD	= 1
M_SUB	= 2
M_MUL	= 3
M_DIV	= 4
M_ROUND	= 17
M_CMP	= 18
M_ITOF	= 19
M_FTOI	= 20

.macro	m_copy4 (src, dst)
	lda :src
	sta :dst
	lda :src+1
	sta :dst+1
	lda :src+2
	sta :dst+2
	lda :src+3
	sta :dst+3
.endm

; F0 = FP1MAN, F1 = FP2MAN, F0 = F0 op F1, FPMAN = F0
.macro	m_binop (op)
	m_copy4 FP1MAN0, M_F0
	m_copy4 FP2MAN0, M_F1
	lda #$01
	sta M_FARG
	lda #%%op
	sta M_FOP
	m_copy4 M_F0, FPMAN0
.endm

.proc	@FADD
	m_binop M_ADD
	rts
.endp

.proc	@FSUB
	m_binop M_SUB
	rts
.endp

.proc	@FMUL
	m_binop M_MUL
	rts
.endp

.proc	@FDIV
	m_binop M_DIV
	rts
.endp

.proc	@FCMPL
A	= FP1MAN0		; the compiler addresses the operands as @FCMPL.A / @FCMPL.B
B	= FPMAN0
	m_copy4 FPMAN0, M_F0		; the answer is the sign of B - A (as the software routine has it)
	m_copy4 FP1MAN0, M_F1
	lda #$01
	sta M_FARG
	lda #M_CMP
	sta M_FOP
	lda M_FLAGS
	lsr @
	bcs equal		; bit0: zero
	lsr @
	bcs less		; bit1: negative
	lda #$01
	rts
less	lda #$FF
	rts
equal	lda #$00
	rts
.endp

.proc	@F2I
	m_copy4 FPMAN0, M_F0
	stz M_FARG
	lda #M_FTOI
	sta M_FOP
	m_copy4 M_FI, FPMAN0
	rts
.endp

.proc	@I2F
	m_copy4 FPMAN0, M_FI
	stz M_FARG
	lda #M_ITOF
	sta M_FOP
	m_copy4 M_F0, FPMAN0
	rts
.endp

.proc	@FROUND
	m_copy4 FP2MAN0, M_F0
	stz M_FARG
	lda #M_ROUND
	sta M_FOP
	lda #M_FTOI
	sta M_FOP
	m_copy4 M_FI, FPMAN0
	rts
.endp

.proc	@FFRAC			; x - trunc(x), the sign of x
	m_copy4 FPMAN0, M_F0
	stz M_FARG
	lda #M_FTOI
	sta M_FOP
	lda #$10		; F1 = (float) FI
	sta M_FARG
	lda #M_ITOF
	sta M_FOP
	lda #$01		; F0 = F0 - F1
	sta M_FARG
	lda #M_SUB
	sta M_FOP
	m_copy4 M_F0, FPMAN0
	rts
.endp

.proc	@NEGINT
	lda #$00
	sec
	sbc FPMAN0
	sta FPMAN0
	lda #$00
	sbc FPMAN1
	sta FPMAN1
	lda #$00
	sbc FPMAN2
	sta FPMAN2
	lda #$00
	sbc FPMAN3
	sta FPMAN3
	rts
.endp
