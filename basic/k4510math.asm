; ---- EhBASIC's float arithmetic on the K4510 MATH unit ($D700) ------------
; EhBASIC keeps Microsoft-format floats: excess-128 exponent (0 = zero),
; 24-bit mantissa 0.1xxx with the leading 1 explicit in FAC1_1 bit 7, sign
; in FAC1_s bit 7.  IEEE single: sign, excess-127 exponent with an implied
; leading 1, 23-bit mantissa.  So: IEEE exponent = e - 2, the 23 bits are
; FAC1_1 & $7F, FAC1_2, FAC1_3 -- a byte shuffle each way.
; ADD, MULTIPLY, DIVIDE (FAC1 = FAC2 op FAC1) and SQR SIN COS TAN ATN EXP LOG
; (FAC1 = f(FAC1)) are redirected here by a JMP at their original labels;
; parsing, comparison, INT and number printing stay EhBASIC's.

MF0	= $D700
MF1	= $D704
MFOP	= $D720
MFARG	= $D721
MFFLAGS	= $D722
MATH_MOV = 0
MATH_ADD = 1
MATH_MUL = 3
MATH_DIV = 4
MATH_SQRT = 5
MATH_SIN = 6
MATH_COS = 7
MATH_TAN = 8
MATH_ATAN = 9
MATH_EXP = 11
MATH_LOG = 12

; FAC1 -> F0  (A, X, Y clobbered)
k_fac1_f0
	LDA	FAC1_e
	BEQ	k_f1f0_zero
	SEC
	SBC	#2			; IEEE exponent
	BCC	k_f1f0_zero		; e < 2: below IEEE single's range here, call it 0
	TAX				; X = ieee exponent
	LDA	FAC1_3
	STA	MF0
	LDA	FAC1_2
	STA	MF0+1
	TXA
	LSR				; C = exponent bit 0
	STA	k_tmp			; exponent >> 1
	LDA	FAC1_1
	AND	#$7F
	BCC	k_f1f0_b2
	ORA	#$80
k_f1f0_b2
	STA	MF0+2
	LDA	FAC1_s
	AND	#$80
	ORA	k_tmp
	STA	MF0+3
	RTS
k_f1f0_zero
	STZ	MF0
	STZ	MF0+1
	STZ	MF0+2
	STZ	MF0+3
	RTS

; FAC2 -> F1
k_fac2_f1
	LDA	FAC2_e
	BEQ	k_f2f1_zero
	SEC
	SBC	#2
	BCC	k_f2f1_zero
	TAX
	LDA	FAC2_3
	STA	MF1
	LDA	FAC2_2
	STA	MF1+1
	TXA
	LSR
	STA	k_tmp
	LDA	FAC2_1
	AND	#$7F
	BCC	k_f2f1_b2
	ORA	#$80
k_f2f1_b2
	STA	MF1+2
	LDA	FAC2_s
	AND	#$80
	ORA	k_tmp
	STA	MF1+3
	RTS
k_f2f1_zero
	STZ	MF1
	STZ	MF1+1
	STZ	MF1+2
	STZ	MF1+3
	RTS

; F0 -> FAC1; overflow error if the result does not fit (inf, NaN, 2^127+)
k_f0_fac1
	LDA	MF0+3
	AND	#$7F
	ASL				; exponent bits 7..1
	STA	k_tmp
	LDA	MF0+2
	ASL				; C = exponent bit 0
	LDA	k_tmp
	ADC	#0			; A = IEEE exponent
	BEQ	k_f0f1_zero		; zero or denormal -> 0
	CMP	#$FE
	BCS	k_f0f1_ovf		; 254, 255: too big for e = exp + 2 (or inf/NaN)
	CLC
	ADC	#2
	STA	FAC1_e
	LDA	MF0+2
	ORA	#$80
	STA	FAC1_1
	LDA	MF0+1
	STA	FAC1_2
	LDA	MF0
	STA	FAC1_3
	LDA	MF0+3
	AND	#$80
	BEQ	k_f0f1_pos
	LDA	#$FF
k_f0f1_pos
	STA	FAC1_s
	STZ	FAC1_r
	RTS
k_f0f1_zero
	STZ	FAC1_e
	STZ	FAC1_s
	STZ	FAC1_r
	RTS
k_f0f1_ovf
	JMP	LAB_2564		; overflow error, warm start

k_tmp	.byte 0
k_op	.byte 0

; FAC1 = FAC2 op FAC1 : the binary operations
K_ADD
	JSR	k_fac1_f0
	JSR	k_fac2_f1
	LDA	#$01			; F0 = F0 + F1
	STA	MFARG
	LDA	#MATH_ADD
	STA	MFOP
	JMP	k_f0_fac1

K_MULTIPLY
	JSR	k_fac1_f0
	JSR	k_fac2_f1
	LDA	#$01
	STA	MFARG
	LDA	#MATH_MUL
	STA	MFOP
	JMP	k_f0_fac1

K_DIVIDE				; FAC1 = FAC2 / FAC1
	LDA	FAC1_e
	BNE	k_div_ok
	JMP	LAB_2737		; divide by zero error
k_div_ok
	JSR	k_fac1_f0
	JSR	k_fac2_f1
	LDA	#$10			; F1 = F1 / F0
	STA	MFARG
	LDA	#MATH_DIV
	STA	MFOP
	LDA	#$01			; F0 = F1 (MOV)
	STA	MFARG
	LDA	#MATH_MOV
	STA	MFOP
	JMP	k_f0_fac1

; FAC1 = f(FAC1) : the functions; X = op
k_unary
	STX	k_op
	JSR	k_fac1_f0
	STZ	MFARG			; F0 = f(F0)
	LDA	k_op
	STA	MFOP
	JMP	k_f0_fac1

K_SQR
	LDA	FAC1_s
	BPL	k_sqr_ok
	JMP	LAB_FCER		; negative: function call error
k_sqr_ok
	LDX	#MATH_SQRT
	BRA	k_unary
K_SIN
	LDX	#MATH_SIN
	BRA	k_unary
K_COS
	LDX	#MATH_COS
	BRA	k_unary
K_TAN
	LDX	#MATH_TAN
	BRA	k_unary
K_ATN
	LDX	#MATH_ATAN
	BRA	k_unary
K_EXP
	LDX	#MATH_EXP
	BRA	k_unary
K_LOG
	LDA	FAC1_e
	BEQ	k_log_err
	LDA	FAC1_s
	BPL	k_log_ok
k_log_err
	JMP	LAB_FCER		; zero or negative: function call error
k_log_ok
	LDX	#MATH_LOG
	BRA	k_unary
