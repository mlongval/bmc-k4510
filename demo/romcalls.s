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
; unsigned char __fastcall__ rom_shell(const char *line)  -- runs a shell command
; A/X = the line, as cc65 passes a pointer.  Lets a program reach the shell for
; the things only it can do (MKDIR, and the rest of K/OS).
        .export _rom_shell
_rom_shell:
        jmp $FF8F
