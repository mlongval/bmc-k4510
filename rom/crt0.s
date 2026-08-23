; BMC-K4510 system ROM startup for cc65. 65C02 code: a strict subset of the 45GS02.
        .export   _exit, __STARTUP__ : absolute = 1
        .import   _main, __DATA_LOAD__, __DATA_RUN__, __DATA_SIZE__, __BSS_RUN__, __BSS_SIZE__
        .import   __RAM_START__, __RAM_SIZE__, __STACKSIZE__, __STK_START__, __STK_SIZE__
        .import   copydata, zerobss, initlib
        .importzp sp
        .import   incsp4
        .import   _k_chrout, _k_chrin, _k_getin, _k_load, _k_save
        .export   _ticks, _cursor_far, _cursor_vis, _speed_loop, _far_poke, _call_prog

        .zeropage
cnt:          .res 2
fp:           .res 4           ; far pointer for the flat forms

        .bss
_ticks:       .res 1
_cursor_vis:  .res 1           ; nonzero while the ROM wants a cursor shown
_cursor_far:  .res 4           ; far address of the attribute byte under the cursor
t0:           .res 1
zp_rom:       .res 32          ; the ROM's zero page $02-$21 while a program runs
zp_tmp:       .res 32
zp_save:      .res 6           ; IRQ scratch

        .segment "STARTUP"
reset:  sei
        cld
        ldx #$FF
        txs
        lda #<(__STK_START__ + __STK_SIZE__)      ; cc65 software stack: $0800 down
        sta sp
        lda #>(__STK_START__ + __STK_SIZE__)
        sta sp+1
        jsr copydata
        jsr zerobss
        jsr initlib
        cli
        jsr _main
_exit:  jmp _exit

; IRQ: pure assembly -- cc65 C code must never run here (it would clobber
; the zero-page temporaries of whatever was interrupted).
; The cursor cell is in far memory; the IRQ borrows $02-$05 for the flat
; pointer and restores them, so it is safe whatever program owns the zero page.
irq:    pha
        lda $D004               ; VICKe IRQSTAT
        pha
        and #1                  ; vblank?
        beq @ack
        inc _ticks
        lda _ticks
        and #31
        bne @ack
        lda _cursor_vis
        beq @ack
        phx
        ldx #3
@sv:    lda $02,x
        sta zp_save,x
        lda _cursor_far,x
        sta $02,x
        dex
        bpl @sv
        .byte $EA               ; NOP prefix: 32-bit flat
        lda ($02)               ; LDA [$02],Z   (Z = 0)
        eor #$80
        .byte $EA
        sta ($02)
        ldx #3
@rs:    lda zp_save,x
        sta $02,x
        dex
        bpl @rs
        plx
@ack:   pla
        sta $D004               ; acknowledge what we saw
        pla
        rti
nmi:    rti

; unsigned speed_loop(void): iterations of a fixed loop during one frame.
; 18 cycles per iteration on the 40.5 MHz timing table (see INFO -c)
_speed_loop:
        lda _ticks
@w:     cmp _ticks              ; wait for a tick edge
        beq @w
        lda _ticks
        sta t0
        stz cnt
        stz cnt+1
@l:     inc cnt
        bne @s
        inc cnt+1
@s:     lda t0
        cmp _ticks
        beq @l
        lda cnt
        ldx cnt+1
        rts

; void __fastcall__ far_poke(unsigned long a, unsigned char v)
_far_poke:
        pha
        ldy #0
        lda (sp),y
        sta fp
        iny
        lda (sp),y
        sta fp+1
        iny
        lda (sp),y
        sta fp+2
        iny
        lda (sp),y
        sta fp+3
        pla
        .byte $EA               ; NOP prefix: 32-bit flat
        sta (fp)                ; STA [fp],Z
        jmp incsp4

; void __fastcall__ call_prog(unsigned addr): run a program that may own the
; whole zero page. The ROM's $02-$1F is kept in zp_rom and swapped back in
; around every jump-table call and on return.
_call_prog:
        sta prog_addr
        stx prog_addr+1
        ldx #31
@s:     lda $02,x
        sta zp_rom,x
        dex
        bpl @s
        jsr go_prog
        ldx #31
@r:     lda zp_rom,x
        sta $02,x
        dex
        bpl @r
        rts
go_prog: jmp (prog_addr)

; a jump-table call from a program: program zp -> zp_tmp, ROM zp in, call,
; ROM zp -> zp_rom, program zp back. A and X carry the argument / result.
zp_in:  sta zp_a
        ldx #31
@a:     lda $02,x
        sta zp_tmp,x
        lda zp_rom,x
        sta $02,x
        dex
        bpl @a
        lda zp_a
        rts
zp_out: sta zp_a
        ldx #31
@b:     lda $02,x
        sta zp_rom,x
        lda zp_tmp,x
        sta $02,x
        dex
        bpl @b
        lda zp_a
        rts
        .bss
zp_a:   .res 1
prog_addr: .res 2
        .segment "CODE"
w_chrout: jsr zp_in
        jsr _k_chrout
        jmp zp_out
w_chrin:  jsr zp_in
        jsr _k_chrin
        jmp zp_out
w_getin:  jsr zp_in
        jsr _k_getin
        jmp zp_out
w_load:   jsr zp_in
        jsr _k_load
        jmp zp_out
w_save:   jsr zp_in
        jsr _k_save
        jmp zp_out

; ---- jump table at $FF80: the system call interface ----
        .segment "JUMPTAB"
        jmp w_chrout            ; $FF80  CHROUT  A = char
        jmp w_chrin             ; $FF83  CHRIN   -> A, blocks
        jmp w_getin             ; $FF86  GETIN   -> A, 0 if none
        jmp w_load              ; $FF89  LOAD    name ptr in $F0/$F1, dest in $F2..$F5 -> A status, size in $F6..$F9
        jmp w_save              ; $FF8C  SAVE    name ptr $F0/$F1, src $F2..$F5, len $F6..$F9 -> A status

        .segment "VECTORS"
        .word nmi
        .word reset
        .word irq
