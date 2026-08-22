; BMC-K4510 system ROM startup for cc65. 65C02 code: a strict subset of the 45GS02.
        .export   _exit, __STARTUP__ : absolute = 1
        .import   _main, __DATA_LOAD__, __DATA_RUN__, __DATA_SIZE__, __BSS_RUN__, __BSS_SIZE__
        .import   __RAM_START__, __RAM_SIZE__, __STACKSIZE__
        .import   copydata, zerobss, initlib
        .importzp sp
        .import   _k_chrout, _k_chrin, _k_getin, _k_load, _k_save
        .export   _ticks, _cursor_cell, _cursor_vis, _speed_loop

        .zeropage
_cursor_cell: .res 2           ; -> attribute byte of the cell under the cursor
cnt:          .res 2

        .bss
_ticks:       .res 1
_cursor_vis:  .res 1           ; nonzero while the ROM wants a cursor shown
t0:           .res 1

        .segment "STARTUP"
reset:  sei
        cld
        ldx #$FF
        txs
        lda #<(__RAM_START__ + __RAM_SIZE__)      ; cc65 software stack: top of RAM area
        sta sp
        lda #>(__RAM_START__ + __RAM_SIZE__)
        sta sp+1
        jsr copydata
        jsr zerobss
        jsr initlib
        cli
        jsr _main
_exit:  jmp _exit

; IRQ: pure assembly -- cc65 C code must never run here (it would clobber
; the zero-page temporaries of whatever was interrupted).
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
        lda (_cursor_cell)      ; 65C02 (zp) = 45GS02 (zp),Z with Z=0
        eor #$80
        sta (_cursor_cell)
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

; ---- jump table at $FF80: the system call interface ----
        .segment "JUMPTAB"
        jmp _k_chrout           ; $FF80  CHROUT  A = char
        jmp _k_chrin            ; $FF83  CHRIN   -> A, blocks
        jmp _k_getin            ; $FF86  GETIN   -> A, 0 if none
        jmp _k_load             ; $FF89  LOAD    name ptr in $F0/$F1, dest in $F2..$F5 -> A status, size in $F6..$F9
        jmp _k_save             ; $FF8C  SAVE    name ptr $F0/$F1, src $F2..$F5, len $F6..$F9 -> A status

        .segment "VECTORS"
        .word nmi
        .word reset
        .word irq
