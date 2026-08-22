; BMC-K4510 system ROM startup for cc65. 65C02 code: a strict subset of the 45GS02.
        .export   _exit, __STARTUP__ : absolute = 1
        .import   _main, __DATA_LOAD__, __DATA_RUN__, __DATA_SIZE__, __BSS_RUN__, __BSS_SIZE__
        .import   __RAM_START__, __RAM_SIZE__, __STACKSIZE__
        .import   copydata, zerobss, initlib
        .importzp sp
        .import   _k_chrout, _k_chrin, _k_getin, _k_load, _k_save, _k_irq_handler

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

irq:    pha
        phx
        phy
        jsr _k_irq_handler
        ply
        plx
        pla
        rti
nmi:    rti

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
