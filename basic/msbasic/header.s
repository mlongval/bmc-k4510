		.segment "HEADER"
.ifdef W65C816SXB
; Disable emulation mode (is left on from the monitor)
.setcpu "65816"
        SEC ;set carry for emulation mode
        XCE ;go into emulation mode
.setcpu "65C02"
        jsr INITUSBSERIAL

        ; Add a small delay to allow monitor to connect the terminal after
        ; starting execution
        ldy #0
        ldx #0
@loop:
        dex
        bne @loop
        dey
        bne @loop
        jmp COLD_START
.endif

.ifdef KBD
        jmp     LE68C
        .byte   $00,$13,$56
.endif
.ifdef AIM65
        jmp     COLD_START
        jmp     RESTART
        .word   AYINT,GIVAYF
.endif
.ifdef SYM1
        jmp     PR_WRITTEN_BY
.endif
