; BMC-K4510 startup for sidplay: a cc65 program that lives UNDER the ROM at
; $E000-$FEFF (block 7 banked to RAM by the K4SG loader), so a C64 tune can
; own $0400-$CFFF. Exit goes through a trampoline in low RAM: the banks must
; be cleared by code that is not itself banked away.
        .export   __STARTUP__ : absolute = 1
        .export   _exit, _far_poke, _far_peek, _far_poke16, _map_window
        .export   _tune_call
        .import   _main, zerobss, initlib, incsp4
        .import   __PRG_START__, __PRG_SIZE__
        .importzp sp, sreg, ptr1

        .zeropage
fp:     .res 4

        .segment "STARTUP"
start:  lda #<(__PRG_START__ + __PRG_SIZE__)
        sta sp
        lda #>(__PRG_START__ + __PRG_SIZE__)
        sta sp+1
        jsr zerobss
        jsr initlib
        jsr _main
_exit:  ldx #tramp_end - tramp - 1      ; the exit trampoline to $0230 (free low RAM; the tune is silent and dead by now)
@c:     lda tramp,x
        sta $0230,x
        dex
        bpl @c
        jmp $0230
tramp:  lda #0                  ; MAP everything off: clears the banks too
        tax
        tay
        .byte $A3, $00          ; LDZ #0
        .byte $5C               ; MAP
        .byte $EA               ; EOM
        rts                     ; to the ROM's call_prog
tramp_end:

; void __fastcall__ tune_call(unsigned addr)  -- the song number in tune_a:
; the player's zero page is $40-$63 (36 bytes, sidplay.cfg); the tune owns the
; rest. Around a call those 36 bytes are swapped for the tune's copy, so both
; sides keep their state. The target lives outside the zero page.
        .export _tune_a
_tune_call:
        sta target
        stx target+1
        ldx #35
@s:     lda $40,x
        sta player_zp,x
        lda tune_zp,x
        sta $40,x
        dex
        bpl @s
        lda _tune_a
        ldx #0
        ldy #0
        jsr dojsr
        ldx #35
@r:     lda $40,x
        sta tune_zp,x
        lda player_zp,x
        sta $40,x
        dex
        bpl @r
        rts
dojsr:  jmp (target)

        .bss
_tune_a:   .res 1
target:    .res 2
player_zp: .res 36
tune_zp:   .res 36

        .segment "CODE"
; ---- the flat-memory helpers, copied from prg0.s (see there) ----
; void __fastcall__ map_window(unsigned long phys)
; MAP CPU $2000-$5FFF (blocks 1-2, 16 KB) onto phys. phys: multiple of 256,
; >= $2000 within its megabyte, bits 16-19 not all ones.
_map_window:
        stx fp                  ; bits 8-15
        lda sreg
        sta fp+1                ; bits 16-23
        lda sreg+1
        asl
        asl
        asl
        asl
        sta fp+2
        lda fp+1
        lsr
        lsr
        lsr
        lsr
        ora fp+2
        sta fp+2                ; megabyte
        lda fp
        sec
        sbc #$20
        sta fp                  ; offset bits 8-15
        lda fp+1
        and #$0F
        sbc #0
        and #$0F
        sta fp+1                ; offset bits 16-19
        lda fp+2
        tay
        ldx #$0F
        .byte $A3, $0F          ; LDZ #$0F
        .byte $5C               ; MAP: megabytes
        lda fp+1
        .byte $4B               ; TAZ  (high half: no blocks)
        lda fp+1
        ora #$60                ; low half: blocks 1,2
        tax
        lda fp
        tay
        .byte $5C               ; MAP: offsets + masks
        .byte $EA               ; EOM
        .byte $A3, $00          ; LDZ #0
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
        sta (fp)                ; STA [fp],Z  (Z = 0)
        jmp incsp4

; void __fastcall__ far_poke16(unsigned long a, unsigned int v)
_far_poke16:
        pha
        phx
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
        plx
        pla
        .byte $EA
        sta (fp)
        txa
        .byte $A3, $01          ; LDZ #1
        .byte $EA
        sta (fp)
        .byte $A3, $00          ; LDZ #0
        jmp incsp4

; unsigned char __fastcall__ far_peek(unsigned long a)
_far_peek:
        sta fp
        stx fp+1
        lda sreg
        sta fp+2
        lda sreg+1
        sta fp+3
        .byte $EA
        lda (fp)                ; LDA [fp],Z
        ldx #0
        rts
