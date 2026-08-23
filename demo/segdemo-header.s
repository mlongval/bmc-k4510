; K4SG header for segdemo (demo/segdemo-header.s): three segments. ld65 writes the memory areas in
; the order of seg.cfg (HDR, PRG, OVL1, OVL2), so the bytes follow the table.
        .export __K4SG__ : absolute = 1
        .import __PRG_START__, __BSS_RUN__, __OVL1_START__, __OVL1_LAST__, __OVL2_START__, __OVL2_LAST__
        .segment "SEGHDR"
        .byte "K4SG"
        .byte 3                         ; segments
        .byte 0                         ; flags
        .word __PRG_START__             ; entry: prg0's start is the first byte of PRG
        ; phys[4] len[4] block pad[3]
        .dword __PRG_START__
        .dword __BSS_RUN__ - __PRG_START__      ; the file holds everything up to BSS (prg0 zeroes BSS)
        .byte $FF, 0, 0, 0              ; main: no bank (it is in the unmapped view)
        .dword $00100000
        .dword __OVL1_LAST__ - __OVL1_START__
        .byte $FF, 0, 0, 0              ; overlays are banked by the gate when called
        .dword $00102000
        .dword __OVL2_LAST__ - __OVL2_START__
        .byte $FF, 0, 0, 0
