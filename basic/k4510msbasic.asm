; Microsoft BASIC for 6502 on the K4510, as a .prg the system ROM loads
; at $7000.  Microsoft released the original source in 2025 under MIT;
; mist64/msbasic is the buildable ca65 reconstruction of it, vendored
; unmodified under msbasic/ (see msbasic/VENDORED-FROM.txt).  This file
; is the whole K4510 port: the configuration, the console glue and the
; .prg header.  Nothing under msbasic/ is edited.
;
; The machine's second native BASIC, beside EhBASIC (basic/basic.asm).
; The two do not share a line: EhBASIC is an MS-alike with the K4510
; extension words (GRAPHICS, PLOT, SPRITE, ...); this is the real thing,
; 1977 vintage, and for now it is the plain interpreter.
;
; Configuration: CONFIG_2C -- the newest Microsoft base in the tree (the
; one MicroTAN forked from), 9-digit floating point, every bugfix up to
; 2C, and none of the OEM additions.  That is the pure-MS build the
; licence research called for (docs/BUILD-LOG.md, 2026-08-24): it rests
; on Microsoft's MIT release alone, with no Commodore or Apple
; reconstruction in it.
;
; Memory map while MS BASIC runs (all inside user RAM):
;   $0000-$00FF  the whole zero page is BASIC's.  The ROM's own zp is
;                $02-$21, but every jump-table stub swaps it in and out
;                (rom/crt0.s: zp_in / zp_out), so we may have all of it
;   $0100-$01FF  the hardware stack; COLD_START resets SP, so there is no
;                returning to the shell (see "Leaving", below)
;   $0200-$07FF  NOT OURS: the ROM's data, bss and C stack
;   $0800-$6FFF  BASIC's program and variable RAM -- 26 KB
;   $6FFC-$6FFF  the .prg header (load address, run address)
;   $7000-.....  this image, loaded by K:OS from /MSBASIC/MSBASIC.PRG
;
; $7000 is where EhBASIC's .prg is documented to land, and it is kept
; here so the two arrangements read the same.  It leaves the RAM above
; the image ($9000-$CFFF) unused: raising the load address is free
; program RAM whenever it is wanted, and costs one number in msbasic.cfg
; plus the matching MEMTOP below.
;
; Console: the ROM jump table -- CHROUT $FF80 and CHRIN $FF83, both
; wrapped to preserve X and Y, which the stubs do not promise.  Ctrl-C
; is the keyboard queue's own break flag at $D103, so a key typed while
; a program runs is not lost the way polling the queue would lose it.
;
; Leaving: there is none yet.  MS BASIC has no BYE, and COLD_START has
; already reset the stack pointer by the time BASIC is up, so the shell's
; frame is gone.  Use the reset chord.  The designed exit is the USR
; vector -- 1977's own vendor hook -- which needs a hook after BASIC's
; init has pointed USR at IQERR; that comes with LOAD/SAVE in the next
; stage.

; ---- the K4510 configuration ---------------------------------------------
; (this is what defines_<machine>.s is for the OEM builds; ours lives out
; here so that nothing under msbasic/ has to be touched)

CONFIG_2C               := 1     ; the newest MS base: all bugfixes, 9-digit FP

CONFIG_PEEK_SAVE_LINNUM := 1     ; PEEK does not clobber LINNUM
CONFIG_SAFE_NAMENOTFOUND := 1    ; check both bytes of the caller's address
CONFIG_SCRTCH_ORDER     := 1     ; where in init SCRTCH is called

; Deliberately NOT set:
;   CONFIG_ROR_WORKAROUND  -- the workaround is for the broken ROR of the
;                             1975/76 6502s.  The 45GS02 has a working one.
;   CONFIG_PRINT_CR        -- BASIC would emit a CR on reaching the last
;                             column, but k_chrout already wraps at COLS
;                             (rom/kernal.c), so that would double-space
;                             every full line.  BASIC still counts the
;                             column, which is what TAB and comma need.
;   CONFIG_MONCOUT_DESTROYS_Y -- our MONCOUT preserves Y itself, which is
;                             cheaper than making BASIC save it at every
;                             call site.
;   CONFIG_CBM_ALL, CONFIG_FILE, CONFIG_CBM1_PATCHES -- Commodore
;                             additions.  Out of scope and out of licence.

; zero page (the CONFIG_2C layout)
ZP_START1 = $17
ZP_START2 = $2F
ZP_START3 = $24
ZP_START4 = $85

; extra zero page variables the non-OEM build still expects
USR             := $0021         ; the USR() vector: JMP <addr>
TXPSV           := $00BA

; constants
STACK_TOP       := $FE
SPACE_FOR_GOSUB := $3E
NULL_MAX        := $F0
WIDTH           := 80            ; the K4510 console is 80x60
WIDTH2          := 56            ; last column a comma tab may start in

; memory layout
RAMSTART2       := $0800         ; BASIC's program RAM starts above the ROM's
MEMTOP          := $7000         ; ... and ends where this image begins

; the monitor entry points BASIC calls; defined in the glue below
MONRDKEY        := k4510_in
MONCOUT         := k4510_out

; ---- the ROM jump table ---------------------------------------------------
ROM_CHROUT      = $FF80
ROM_CHRIN       = $FF83          ; blocks until a key
KBD_BREAK       = $D103          ; Ctrl-C / RUN-STOP seen anywhere in the queue
                                 ; (reading takes it; other keys stay for GET)
K_CR            = $0D
K_LF            = $0A

; ---- the .prg header ------------------------------------------------------
; K:OS loads the image and JSRs to the run address.
        .segment "PRGHDR"
        .word   $7000            ; load address
        .word   k4510_start      ; run address

; ---- Microsoft BASIC ------------------------------------------------------
        .include "msbasic.s"

; ---- the K4510 glue -------------------------------------------------------
        .segment "CODE"

k4510_start:
        cld
        ldy     #0
@banner:
        lda     k4510_banner,y
        beq     @go
        jsr     k4510_out
        iny
        bne     @banner
@go:
        jmp     COLD_START

k4510_banner:
        .byte   "MICROSOFT BASIC ON THE K4510", K_CR, K_LF, 0

; ---- console --------------------------------------------------------------
; Out: A = character.  The ROM's k_chrout makes a full newline of CR *and*
; of LF, so the LF of BASIC's CR/LF pair has to be dropped here or every
; line would be double spaced.  (EhBASIC's glue does the same thing.)
k4510_out:
        cmp     #K_LF
        beq     @done
        sta     k4510_ch
        txa
        pha
        tya
        pha
        lda     k4510_ch
        jsr     ROM_CHROUT
        pla
        tay
        pla
        tax
        lda     k4510_ch
@done:
        rts

k4510_ch:     .byte 0           ; the character in flight (A across the call)

; In: blocking, returns the key in A.  Enter arrives as CR ($0D), which is
; what BASIC wants, so no translation -- but lower case is folded up: the
; 1977 tokenizer only knows upper-case keywords.
;
; **This routine echoes, and it must.**  MS BASIC never echoes what is
; typed: INLIN reads through GETLN -> MONRDKEY and prints nothing back
; (msbasic/inline.s), because on a KIM or a PET it was the monitor's input
; routine that echoed.  The K4510's does not -- the ROM's own echoing lives
; in readline(), which BASIC bypasses by calling CHRIN directly.  Without
; the echo here you type and the screen stays empty, which does not read as
; "no echo", it reads as a machine ignoring the keyboard.
;
; Backspace is the same story from the other end.  BASIC's delete character
; is "_" ($5F) and its handler is a bare DEX -- it erases nothing on the
; glass (msbasic/inline.s, L2420).  The host's Backspace ($08) is below $20,
; so BASIC discards it outright.  So: translate $08 to "_" and do the
; destructive erase (BS, space, BS) ourselves.  "@" still kills the whole
; line, as it did in 1977.
;
; While k4510_feed is set, keys come from k4510_answer instead of the
; keyboard.  That is how the "MEMORY SIZE?" prompt gets answered: BASIC's
; own alternative is to walk RAM upwards probing for the top, and the walk
; would march straight through this image.  Feeding it the number is the
; documented way to say where memory ends, and it is one line of canned
; input rather than a fork of init.s.
k4510_in:
        txa
        pha
        tya
        pha
        lda     k4510_feed
        beq     @live
        ldx     k4510_feedx
        lda     k4510_answer,x
        beq     @feed_done
        inc     k4510_feedx
        jmp     @echo    ; echoed too: the boot shows what it answered
@feed_done:
        lda     #0
        sta     k4510_feed
@live:
        jsr     ROM_CHRIN
        cmp     #$08            ; host Backspace: BASIC's delete is "_", and
        bne     @fold           ; nothing but this routine erases the glass
        jsr     k4510_rubout
        lda     #$5F            ; "_" -- echoed already, destructively
        jmp     @out
@fold:
        cmp     #'a'
        bcc     @echo
        cmp     #'z'+1
        bcs     @echo
        and     #$DF            ; fold to upper case
@echo:
        jsr     k4510_out       ; BASIC will not; see above
@out:
        sta     k4510_ch
        pla
        tay
        pla
        tax
        lda     k4510_ch
        rts

; erase the character to the left: BS, space, BS.  At column 0 the ROM's
; k_chrout ignores the backspace, so this cannot walk off the line.
k4510_rubout:
        lda     #$08
        jsr     k4510_out
        lda     #' '
        jsr     k4510_out
        lda     #$08
        jmp     k4510_out

k4510_feed:   .byte 1
k4510_feedx:  .byte 0
; The two questions CONFIG_2C asks at cold start, answered in order:
;   MEMORY SIZE?    28672 = $7000, where this image begins (keep in step
;                   with MEMTOP above)
;   TERMINAL WIDTH? 80, the K4510 console
k4510_answer: .byte "28672", K_CR, "80", K_CR, 0

; ---- Ctrl-C ---------------------------------------------------------------
; Called once per statement.  No break: return with A non-zero (Z clear),
; which is what the caller's "bne RET1" wants.  Break: enter STOP with
; Z and C set, exactly as the OEM versions fall into it.
ISCNTC:
        lda     KBD_BREAK
        bne     @break
        lda     #$01
        rts
@break:
        lda     #$03
        cmp     #$03            ; Z=1, C=1
        jmp     STOP

; ---- LOAD / SAVE ----------------------------------------------------------
; Not this stage.  The ROM has both ($FF89 / $FF8C, name pointer in $F0/$F1,
; a 28-bit address in $F2..$F5, the length in $F6..$F9), so wiring them is
; a contained job -- but it is the job after this one.  Say so rather than
; raise a misleading ?SYNTAX ERROR.
LOAD:
SAVE:
        lda     #<QT_NO_LOADSAVE
        ldy     #>QT_NO_LOADSAVE
        jsr     STROUT
        rts

QT_NO_LOADSAVE:
        .byte   "NOT YET ON THIS BASIC -- USE EHBASIC", K_CR, K_LF, 0
