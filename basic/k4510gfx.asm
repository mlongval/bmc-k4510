; ---- K4510 graphics commands for EhBASIC ---------------------------------
; GRAPHICS n   0 = off, 1 = 320x240, 2 = 640x480 (8 bpp bitmap on VICKe layer 1,
;              above the text; index 0 is transparent; clears the bitmap)
; GCLS         clear the bitmap
; PLOT x,y,c   LINE x1,y1,x2,y2,c   TRI x1,y1,x2,y2,x3,y3,c   (blitter ops)
; PALETTE i,r,g,b
; Arguments are evaluated with EhBASIC's own expression code into gargs.

GFX_BUF		= $200000		; the bitmap, far memory (640*480 max)
VK		= $D000
DMA		= $D200

gargs	= $03A0			; 8 x 16-bit argument slots (above EhBASIC's Ibuffs, which ends at $039F)
gmode	= $03B0			; current GRAPHICS mode
ROM_VIDEO = $FF92		; ROM: restore the text screen mode and palette
gw	= $03B1			; width low/high
gh	= $03B3

; get N comma-separated integer arguments into gargs
k_getargs				; X = count
	STX	gcount
	LDX	#0
k_ga_loop
	PHX
	JSR	LAB_EVNM		; evaluate, must be numeric
	JSR	LAB_F2FX		; to Itempl/h (handles negative)
	PLX
	LDA	Itempl
	STA	gargs,X
	LDA	Itemph
	STA	gargs+1,X
	INX
	INX
	TXA
	LSR
	CMP	gcount
	BCS	k_ga_done
	JSR	LAB_1C01		; scan for "," else syntax error
	BRA	k_ga_loop
k_ga_done
	RTS
gcount	.byte 0

; set up the blitter for the current surface; A = op
k_blt_setup
	PHA
	LDA	#<GFX_BUF
	STA	VK+$74
	LDA	#>GFX_BUF
	STA	VK+$75
	LDA	#^GFX_BUF
	STA	VK+$76
	STZ	VK+$77
	LDA	gw
	STA	VK+$78			; BLTW
	STA	VK+$7E			; BLTDS (stride = width, 8 bpp)
	LDA	gw+1
	STA	VK+$79
	STA	VK+$7F
	LDA	gh
	STA	VK+$7A			; BLTH
	LDA	gh+1
	STA	VK+$7B
	PLA
	STA	VK+$80			; BLTOP
	RTS

K_GRAPHICS
	JSR	LAB_EVNM
	JSR	LAB_F2FX
	LDA	Itempl
	BNE	k_gfx_some
	STA	gmode
	JMP	k_gfx_off
k_gfx_some
	STA	gmode
	CMP	#1
	BEQ	k_gfx_lo
	LDA	#<640
	STA	gw
	LDA	#>640
	STA	gw+1
	LDA	#<480
	STA	gh
	LDA	#>480
	STA	gh+1
	LDA	#$01			; CTRL: display on, full res
	JMP	k_gfx_on
k_gfx_lo
	LDA	#<320
	STA	gw
	LDA	#>320
	STA	gw+1
	LDA	#<240
	STA	gh
	STZ	gh+1
	LDA	#$03			; CTRL: display on, 320x240
k_gfx_on
	STA	VK+$00
	; layer 1: bitmap, 8 bpp, stride = width, data = GFX_BUF
	STZ	VK+$21			; palofs
	STZ	VK+$22			; scroll
	STZ	VK+$23
	STZ	VK+$24
	STZ	VK+$25
	LDA	gw
	STA	VK+$26
	LDA	gw+1
	STA	VK+$27
	LDA	#<GFX_BUF
	STA	VK+$28
	LDA	#>GFX_BUF
	STA	VK+$29
	LDA	#^GFX_BUF
	STA	VK+$2A
	STZ	VK+$2B
	LDA	#$19			; enable | bitmap | 8 bpp
	STA	VK+$20
	; fall into GCLS
K_GCLS
	LDA	gmode
	BEQ	k_gfx_rts
	STZ	DMA+0			; fill value 0
	LDA	#<GFX_BUF
	STA	DMA+4
	LDA	#>GFX_BUF
	STA	DMA+5
	LDA	#^GFX_BUF
	STA	DMA+6
	STZ	DMA+7
	; length = gw * gh: 320*240 = 76800 ($12C00), 640*480 = 307200 ($4B000)
	LDA	gmode
	CMP	#1
	BEQ	k_len_lo
	STZ	DMA+8
	LDA	#$B0
	STA	DMA+9
	LDA	#$04
	STA	DMA+10
	BRA	k_len_go
k_len_lo
	STZ	DMA+8
	LDA	#$2C
	STA	DMA+9
	LDA	#$01
	STA	DMA+10
k_len_go
	STZ	DMA+11
	LDA	#2
	STA	DMA+12			; DMA fill
k_gfx_rts
	RTS
k_gfx_off
	STZ	VK+$20			; layer 1 off
	JMP	ROM_VIDEO		; the ROM puts its mode and palette back (the demo may have changed both)
	RTS

; PLOT x,y,c : a one-pixel LINE from (x,y) to (x,y)
K_PLOT
	LDX	#3
	JSR	k_getargs
	LDA	gargs+4			; colour
	STA	gargs+8
	LDA	gargs+0
	STA	gargs+4
	LDA	gargs+1
	STA	gargs+5
	LDA	gargs+2
	STA	gargs+6
	LDA	gargs+3
	STA	gargs+7
	BRA	k_line_go
K_LINE
	LDX	#5
	JSR	k_getargs
k_line_go
	LDA	gargs+8			; colour
	STA	VK+$70			; BLTSRC byte 0 = colour
	LDA	#6
	JSR	k_blt_setup
	LDX	#7
k_lc	LDA	gargs,X
	STA	VK+$84,X
	DEX
	BPL	k_lc
	STA	VK+$82			; go
	RTS

K_TRI
	LDX	#7
	JSR	k_getargs
	LDA	gargs+12		; colour
	STA	VK+$70
	LDA	#7
	JSR	k_blt_setup
	LDX	#11
k_tc	LDA	gargs,X
	STA	VK+$84,X
	DEX
	BPL	k_tc
	STA	VK+$82
	RTS

K_PALETTE
	LDX	#4
	JSR	k_getargs
	LDA	gargs+0
	STA	VK+$06			; PALIDX
	LDA	gargs+2
	STA	VK+$07
	LDA	gargs+4
	STA	VK+$08
	LDA	gargs+6
	STA	VK+$09			; commits
	RTS
