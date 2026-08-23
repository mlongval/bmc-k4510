; ---- Expressions compiled to math lists (EhBASIC level 2) ----------------
; When EhBASIC evaluates an expression inside a program, the expression is
; compiled once into a math list -- constants as LDF immediates, variables
; as LDMS loads from their addresses, operators and functions as register
; ops on a seven-deep register stack -- and cached by the expression's
; address in the program.  Later evaluations run the list: one write.
; Anything the compiler does not handle (strings, arrays, RND, PEEK,
; comparisons, user functions ...) is left to the interpreter, which still
; does the arithmetic on the MATH unit (k4510math.asm).  The cache is
; emptied whenever the program or the variables move (RUN, CLEAR, NEW,
; line entry).  Immediate-mode expressions are never cached.

CACHE	= $0D0000		; far: 256 entries x 8 bytes (key lo/hi, end lo/hi, list 24-bit, valid)
ARENA	= $0D1000		; far: the lists, bump-allocated up to ARENA_END
ARENA_END = $0E0000

MLPTR	= $D728
MLRUN	= $D72C
MLSTAT	= $D72D

; zero page $03-$09 is ours while BASIC runs
lp	= $03			; list write pointer (32-bit) / cache entry pointer
tp	= $07			; 16-bit scratch pointer
; page 4 ($040D-$043F free)
k_busy	= $0410			; compiling (or disabled): bypass the hook
k_sp	= $0411			; hardware stack pointer at compile start, for bailing out
k_bp0	= $0412			; Bpntr at entry (2)
k_ksp	= $0414			; register stack depth
k_prec	= $0415
k_arena	= $0416			; next free arena byte (3)
k_hash	= $0419
k_init	= $041A			; arena/cache initialised once
k_nok	= $041B			; compiled expressions (PEEK 1051)
k_nfail	= $041C			; bailed (PEEK 1052)
k_nhit	= $041D			; cache hits (PEEK 1053)
k_norig	= $041E			; went to the interpreter without trying (PEEK 1054)
k_nbad	= $041F			; list ran but did not END (PEEK 1055)

; ---- the hook: JMP here from LAB_EVEX ----
K_EVEX
	LDA	k_busy
	BNE	k_ev_orig
	LDA	Bpntrh
	CMP	#$08
	BCC	k_ev_orig		; immediate mode: not cached
	LDA	k_init
	BNE	k_ev_ready
	JSR	K_INVAL
	INC	k_init
k_ev_ready
	; cache lookup: entry = CACHE + ((lo ^ hi) * 8)
	LDA	Bpntrl
	EOR	Bpntrh
	STA	k_hash
	JSR	k_entry_ptr		; lp -> entry
	JSR	k_entry_get		; k_ent = the 8 bytes
	LDA	k_ent+7
	BEQ	k_ev_compile
	LDA	k_ent
	CMP	Bpntrl
	BNE	k_ev_compile
	LDA	k_ent+1
	CMP	Bpntrh
	BNE	k_ev_compile
	; hit: run the list
	INC	k_nhit
	LDA	k_ent+4
	STA	MLPTR
	LDA	k_ent+5
	STA	MLPTR+1
	LDA	k_ent+6
	STA	MLPTR+2
	STZ	MLPTR+3
	STA	MLRUN
	LDA	MLSTAT
	BEQ	k_ev_ran
	INC	k_nbad
	BRA	k_ev_orig		; a list that did not END cleanly: let the interpreter do it
k_ev_ran
	LDA	k_ent+2
	STA	Bpntrl
	LDA	k_ent+3
	STA	Bpntrh
	JSR	k_f0_fac1
	STZ	Dtypef
	RTS
k_ev_orig
	INC	k_norig
	JMP	LAB_EVEX_SW

k_ev_compile
	INC	k_busy
	TSX
	STX	k_sp
	LDA	Bpntrl
	STA	k_bp0
	LDA	Bpntrh
	STA	k_bp0+1
	STZ	k_ksp
	LDA	k_arena			; lp = arena
	STA	lp
	LDA	k_arena+1
	STA	lp+1
	LDA	k_arena+2
	STA	lp+2
	STZ	lp+3
	LDA	#0
	JSR	k_expr			; compile; bails to k_fail on anything unknown
	JSR	LAB_GBYT		; the terminator must be one the interpreter would stop at too
	BEQ	k_term_ok		; end of line
	CMP	#':'
	BEQ	k_term_ok
	CMP	#','
	BEQ	k_term_ok
	CMP	#')'
	BEQ	k_term_ok
	CMP	#';'
	BEQ	k_term_ok
	CMP	#TK_THEN
	BEQ	k_term_ok
	CMP	#TK_TO
	BEQ	k_term_ok
	CMP	#TK_STEP
	BEQ	k_term_ok
	CMP	#TK_GOTO
	BEQ	k_term_ok
	CMP	#TK_GOSUB
	BEQ	k_term_ok
	JMP	k_fail
k_term_ok
	LDA	k_ksp
	CMP	#1
	BNE	k_fail_j		; exactly one value should be left
	LDA	#$80
	JSR	k_emit
	LDA	#0
	JSR	k_emit
	LDA	lp+2			; arena overflow?
	CMP	#^ARENA_END
	BCS	k_fail_j
	; store the cache entry
	LDA	k_bp0
	STA	k_ent
	LDA	k_bp0+1
	STA	k_ent+1
	LDA	Bpntrl
	STA	k_ent+2
	LDA	Bpntrh
	STA	k_ent+3
	LDA	k_arena
	STA	k_ent+4
	LDA	k_arena+1
	STA	k_ent+5
	LDA	k_arena+2
	STA	k_ent+6
	LDA	#1
	STA	k_ent+7
	LDA	lp			; arena = lp (the list just written)
	STA	k_arena
	LDA	lp+1
	STA	k_arena+1
	LDA	lp+2
	STA	k_arena+2
	JSR	k_entry_ptr
	JSR	k_entry_put
	INC	k_nok
	STZ	k_busy
	; restore Bpntr to the entry value and take the hit path
	LDA	k_bp0
	STA	Bpntrl
	LDA	k_bp0+1
	STA	Bpntrh
	JMP	K_EVEX
k_fail_j
	JMP	k_fail

; bail out of a compile: unwind the stack, restore the text pointer, interpret
k_fail
	INC	k_nfail
	LDX	k_sp
	TXS
	LDA	k_bp0
	STA	Bpntrl
	LDA	k_bp0+1
	STA	Bpntrh
	STZ	k_busy
	JMP	LAB_EVEX_SW

; ---- cache helpers ----
k_entry_ptr				; lp = CACHE + k_hash * 8
	LDA	k_hash
	ASL
	STA	lp
	LDA	#0
	ROL
	ASL	lp
	ROL
	ASL	lp
	ROL
	CLC
	ADC	#>CACHE
	STA	lp+1
	LDA	#^CACHE
	STA	lp+2
	STZ	lp+3
	RTS
k_entry_get				; k_ent[0..7] = [lp..lp+7]
	LDY	#0
k_eg_l
	.byte	$EA
	LDA	(lp)			; LDA [lp],Z is not indexable by Y; walk lp instead
	STA	k_ent,Y
	INC	lp
	INY
	CPY	#8
	BNE	k_eg_l
	RTS
k_entry_put
	LDY	#0
k_ep_l
	LDA	k_ent,Y
	.byte	$EA
	STA	(lp)
	INC	lp
	INY
	CPY	#8
	BNE	k_ep_l
	RTS
k_ent	.res 8

; empty the cache and the arena (hooked into LAB_1477 and LAB_1319)
K_INVAL
	PHA
	PHX
	PHY
	LDA	#<ARENA
	STA	k_arena
	LDA	#>ARENA
	STA	k_arena+1
	LDA	#^ARENA
	STA	k_arena+2
	; DMA fill the 2 KB cache table with 0
	STZ	$D200
	STZ	$D204
	LDA	#>CACHE
	STA	$D205
	LDA	#^CACHE
	STA	$D206
	STZ	$D207
	STZ	$D208
	LDA	#$08
	STA	$D209
	STZ	$D20A
	STZ	$D20B
	LDA	#2
	STA	$D20C
	PLY
	PLX
	PLA
	RTS

; ---- emitting ----
k_emit					; A -> [lp++]
	.byte	$EA
	STA	(lp)
	INC	lp
	BNE	k_em_d
	INC	lp+1
	BNE	k_em_d
	INC	lp+2
k_em_d
	RTS

; push: load into register k_ksp (fails at 7)
k_reg_push				; returns A = register number
	LDA	k_ksp
	CMP	#7
	BCS	k_fail_j2
	INC	k_ksp
	RTS
k_fail_j2
	JMP	k_fail

; emit a binary op X: Fd = Fd op Fs with d = ksp-2, s = ksp-1; ksp--
k_binop
	TXA
	JSR	k_emit
	LDA	k_ksp
	SEC
	SBC	#2
	ASL
	ASL
	ASL
	ASL
	ORA	k_ksp
	DEC				; (ksp-2)<<4 | (ksp-1)
	JSR	k_emit
	DEC	k_ksp
	RTS

; emit a unary op X on the top register: Fd = f(Fd)
k_unop
	TXA
	JSR	k_emit
	LDA	k_ksp
	DEC
	STA	k_tmp3
	ASL
	ASL
	ASL
	ASL
	ORA	k_tmp3
	JSR	k_emit
	RTS
k_tmp3	.byte 0

; ---- the parser: expr(min precedence in A) ----
; precedence: + - = 1, * / = 2, ^ = 3
k_expr
	PHA				; min precedence
	JSR	k_prim
k_ex_loop
	JSR	LAB_GBYT
	LDX	#1
	CMP	#TK_PLUS
	BEQ	k_ex_op
	CMP	#TK_MINUS
	BEQ	k_ex_op
	LDX	#2
	CMP	#TK_MUL
	BEQ	k_ex_op
	CMP	#TK_DIV
	BEQ	k_ex_op
	LDX	#3
	CMP	#TK_POWER
	BEQ	k_ex_op
	PLA
	RTS
k_ex_op
	STA	k_tok
	PLA				; min precedence
	PHA
	STA	k_prec
	CPX	k_prec
	BCC	k_ex_done		; operator binds less tightly: return
	; consume the operator, parse the right side at precedence X+1 (X for ^: right-assoc)
	LDA	k_tok
	PHA				; the operator token survives the recursion on the stack
	PHX
	JSR	LAB_IGBY
	PLX
	TXA
	INC				; right side at precedence + 1 (left-associative, EhBASIC's way, for ^ too)
	PHX
	JSR	k_expr
	PLX
	PLA
	STA	k_tok2
	CPX	#3
	BEQ	k_ex_pow
	CPX	#2
	BEQ	k_ex_muldiv
	LDA	k_tok2
	CMP	#TK_PLUS
	BEQ	k_ex_add
	LDX	#2			; SUB
	BRA	k_ex_emit
k_ex_add
	LDX	#1
	BRA	k_ex_emit
k_ex_muldiv
	LDA	k_tok2
	CMP	#TK_MUL
	BEQ	k_ex_mul
	LDX	#4			; DIV
	BRA	k_ex_emit
k_ex_mul
	LDX	#3
	BRA	k_ex_emit
k_ex_pow
	LDX	#13			; POW
k_ex_emit
	JSR	k_binop
	BRA	k_ex_loop
k_ex_done
	PLA
	RTS
k_tok	.byte 0
k_tok2	.byte 0

; unary: - expr | ( expr ) | number | variable | function( expr ) | PI | TWOPI
k_prim
	JSR	LAB_GBYT
	BCC	k_pr_number		; digit
	CMP	#'.'
	BEQ	k_pr_number
	CMP	#TK_MINUS
	BEQ	k_pr_neg
	CMP	#'('
	BEQ	k_pr_paren
	CMP	#TK_PI
	BEQ	k_pr_pi
	CMP	#TK_TWOPI
	BEQ	k_pr_twopi
	CMP	#TK_SQR
	BEQ	k_pr_fnj
	CMP	#TK_SIN
	BEQ	k_pr_fnj
	CMP	#TK_COS
	BEQ	k_pr_fnj
	CMP	#TK_TAN
	BEQ	k_pr_fnj
	CMP	#TK_ATN
	BEQ	k_pr_fnj
	CMP	#TK_EXP
	BEQ	k_pr_fnj
	CMP	#TK_LOG
	BEQ	k_pr_fnj
	CMP	#TK_ABS
	BEQ	k_pr_fnj
	CMP	#TK_INT
	BEQ	k_pr_fnj
	JSR	LAB_CASC		; letter?
	BCS	k_pr_varj
	JMP	k_fail
k_pr_fnj
	JMP	k_pr_fn
k_pr_varj
	JMP	k_pr_var

k_pr_neg
	JSR	LAB_IGBY
	LDA	#3
	JSR	k_expr			; binds like ^ : -2^2 = -(2^2)
	LDX	#15			; NEG
	JMP	k_unop

k_pr_paren
	JSR	LAB_IGBY
	LDA	#0
	JSR	k_expr
	JSR	LAB_GBYT
	CMP	#')'
	BEQ	k_pr_close
	JMP	k_fail
k_pr_close
	JMP	LAB_IGBY		; consume ')'

k_pr_number
	JSR	LAB_GBYT		; A = first char, C clear for a digit, as LAB_2887 expects
	JSR	LAB_2887		; EhBASIC's number parser: FAC1, Bpntr after it
	BRA	k_pr_ldf
k_pr_pi
	JSR	LAB_IGBY
	LDA	#<LAB_2C7C
	LDY	#>LAB_2C7C
	JSR	LAB_UFAC		; FAC1 = 2*pi ...
	DEC	FAC1_e			; ... halved = pi (as LAB_PI does)
	BRA	k_pr_ldf
k_pr_twopi
	JSR	LAB_IGBY
	LDA	#<LAB_2C7C
	LDY	#>LAB_2C7C
	JSR	LAB_UFAC
k_pr_ldf				; emit LDF reg, FAC1 as IEEE
	JSR	k_fac1_f0		; F0 = FAC1 (the unit converts for us)
	JSR	k_reg_push
	ASL
	ASL
	ASL
	ASL
	PHA
	LDA	#$88
	JSR	k_emit
	PLA
	JSR	k_emit
	LDA	MF0
	JSR	k_emit
	LDA	MF0+1
	JSR	k_emit
	LDA	MF0+2
	JSR	k_emit
	LDA	MF0+3
	JMP	k_emit

k_pr_fn
	STA	k_fn
	JSR	LAB_IGBY		; past the "FN(" token
	LDA	#0
	JSR	k_expr
	JSR	LAB_GBYT
	CMP	#')'
	BEQ	k_fn_close
	JMP	k_fail
k_fn_close
	JSR	LAB_IGBY
	LDA	k_fn
	LDX	#MATH_SQRT
	CMP	#TK_SQR
	BEQ	k_fn_emit
	LDX	#MATH_SIN
	CMP	#TK_SIN
	BEQ	k_fn_emit
	LDX	#MATH_COS
	CMP	#TK_COS
	BEQ	k_fn_emit
	LDX	#MATH_TAN
	CMP	#TK_TAN
	BEQ	k_fn_emit
	LDX	#MATH_ATAN
	CMP	#TK_ATN
	BEQ	k_fn_emit
	LDX	#MATH_EXP
	CMP	#TK_EXP
	BEQ	k_fn_emit
	LDX	#MATH_LOG
	CMP	#TK_LOG
	BEQ	k_fn_emit
	LDX	#14			; ABS
	CMP	#TK_ABS
	BEQ	k_fn_emit
	LDX	#16			; INT = FLOOR
k_fn_emit
	JMP	k_unop
k_fn	.byte 0

; variable: letters/digits, then not '$' and not '(' ; LAB_GVAR gives the address
k_pr_var
	LDA	Bpntrl			; look ahead for $ or (
	STA	tp
	LDA	Bpntrh
	STA	tp+1
	LDY	#0
k_var_scan
	LDA	(tp),Y
	INY
	BEQ	k_var_fail
	JSR	LAB_CASC
	BCS	k_var_scan
	CMP	#'0'
	BCC	k_var_end
	CMP	#'9'+1
	BCC	k_var_scan
k_var_end
	CMP	#'$'
	BEQ	k_var_fail
	CMP	#'('
	BEQ	k_var_fail
	JSR	LAB_GVAR		; creates it if new; Cvaral/h = address; text pointer after the name
	LDA	Dtypef
	BNE	k_var_fail
	JSR	k_reg_push
	ASL
	ASL
	ASL
	ASL
	PHA
	LDA	#$8A
	JSR	k_emit
	PLA
	JSR	k_emit
	LDA	Cvaral
	JSR	k_emit
	LDA	Cvarah
	JSR	k_emit
	LDA	#0
	JSR	k_emit
	LDA	#0
	JMP	k_emit
k_var_fail
	JMP	k_fail
