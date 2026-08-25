; Calls into the system ROM's jump table, for cc65 programs.
        .export _rom_chrout, _rom_getin
; void __fastcall__ rom_chrout(unsigned char c)   -- A = c
_rom_chrout:
        jmp $FF80
; unsigned char rom_getin(void)  -> 0 if no key
_rom_getin:
        jsr $FF86
        ldx #0
        rts
