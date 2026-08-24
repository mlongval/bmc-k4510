; ---- number output and POWER on the MATH unit (stage 3's 47K diet) -------
; FAC1 -> ASCII at Decss+1 (null terminated), AY -> Decssp1. The device
; formats in the exact MS 9-digit style; K_FOUT adds the leading " "/"-",
; K_FOUTR does not (LIST's line numbers enter there with A=0, Y=0).
K_FOUT
	LDX	#MATH_FTOA
	BRA	k_fout
K_FOUTR
	LDX	#MATH_FTOAR
k_fout
	STX	k_op
	JSR	k_fac1_f0
	LDA	#<Decssp1
	STA	MFSPTR
	LDA	#>Decssp1
	STA	MFSPTR+1
	STZ	MFSPTR+2
	STZ	MFSPTR+3
	STZ	MFARG			; source: F0
	LDA	k_op
	STA	MFOP
	LDA	#<Decssp1
	LDY	#>Decssp1
	RTS

K_POWER					; FAC1 = FAC2 ^ FAC1
	JSR	k_fac1_f0
	JSR	k_fac2_f1
	LDA	#$10			; F1 = F1 ^ F0
	STA	MFARG
	LDA	#MATH_POWOP
	STA	MFOP
	LDA	#$01			; F0 = F1
	STA	MFARG
	LDA	#MATH_MOV
	STA	MFOP
	JMP	k_f0_fac1

k4510_banner
	.byte	CR, "BMC-K4510  EhBASIC 2.22 +GRAPHICS  (RUN/STOP: shell)", CR, 0
