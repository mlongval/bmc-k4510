/* K4510 I/O page: $D000-$DFFF in the unmapped CPU view.
 *
 * This layout is the agreed K4510 I/O map (design doc, Plan §3,
 * 2026-08-22). Every device is reached through its base constant.
 */
#ifndef K4510_IO_H
#define K4510_IO_H
#include <stdint.h>

#define IO_BASE        0xD000u
#define IO_VICKE       0xD000u   /* $D000-$D0FF  (Phase 4a)              */
#define IO_INPUT       0xD100u   /* $D100-$D1FF  keyboard, joysticks     */
#define IO_DMA         0xD200u   /* $D200-$D2FF  block DMA (C-18)        */
#define IO_STORAGE     0xD300u   /* $D300-$D3FF  host filesystem (D-09)  */
#define IO_SID         0xD400u   /* $D400-$D47F  4 x SID                 */
#define IO_FM          0xD480u   /* $D480-$D4FF  OPL2, DigiMAX           */
#define IO_SYS         0xD500u   /* $D500-$D5FF  system: clock, RTC, version  */
#define IO_MATH        0xD700u   /* $D700-$D7FF  math unit: float registers + MEGA65-style mul/div */
/* MATH unit. Results are ready the cycle after the write that triggers them.
 *   $D700-$D71F  F0..F7   eight IEEE-754 single registers, little-endian, read/write
 *   $D720  FOP     write = execute; low 5 bits op, see below
 *   $D721  FARG    (dst << 4) | src, register numbers 0..7, write before FOP
 *   $D722  FFLAGS  from the last op: bit0 result zero, bit1 negative, bit2 NaN/inf
 *   $D724-$D727  FI  int32 for ITOF / FTOI
 *   ops: 0 MOV Fd=Fs  1 ADD  2 SUB  3 MUL  4 DIV   (Fd = Fd op Fs)
 *        5 SQRT 6 SIN 7 COS 8 TAN 9 ATAN 10 EXP 11 LOG 14 ABS 15 NEG 16 FLOOR 17 ROUND  (Fd = f(Fs))
 *        10 ATAN2 (Fd = atan2(Fd,Fs))  13 POW (Fd = Fd^Fs)  21 FMOD (Fd = fmod(Fd,Fs))
 *        18 CMP   flags from Fd - Fs, registers unchanged
 *        19 ITOF  Fd = (float)FI        20 FTOI  FI = (int32)Fs, truncated
 *   MEGA65-compatible integer unit (same addresses as the MEGA65):
 *   $D770-$D773 MULTINA  $D774-$D777 MULTINB  (unsigned 32-bit, LE)
 *   $D778-$D77F MULTOUT  = A * B, 64-bit
 *   $D76C-$D76F DIVOUT integer part of A / B   $D768-$D76B fractional part (32.32)
 *   recomputed on every write to an input byte; B = 0 gives all-ones. */
#define MATH_MOV 0
#define MATH_ADD 1
#define MATH_SUB 2
#define MATH_MUL 3
#define MATH_DIV 4
#define MATH_SQRT 5
#define MATH_SIN 6
#define MATH_COS 7
#define MATH_TAN 8
#define MATH_ATAN 9
#define MATH_ATAN2 10
#define MATH_EXP 11
#define MATH_LOG 12
#define MATH_POW 13
#define MATH_ABS 14
#define MATH_NEG 15
#define MATH_FLOOR 16
#define MATH_ROUND 17
#define MATH_CMP 18
#define MATH_ITOF 19
#define MATH_FTOI 20
#define MATH_FMOD 21
/* SYS registers (read-only unless noted):
 *   $00,01  CPU clock, kHz, LE (40500)      $02,03  physical RAM, MB, LE (256)
 *   $04     read: latch the host clock into $05-$0C and return 0
 *   $05 sec $06 min $07 hour $08 day $09 month $0A,0B year LE $0C weekday (0=Sun)
 *   $0D,0E,0F  frames since reset, 24-bit LE (vblank count)
 *   $10-$1F  version string, NUL-terminated
 *   $20     ROM base page (e.g. $A0 for a 24 KB ROM)
 * SID registers $00-$18 read back the last value written (a shadow; real
 * SIDs are write-only there). $19-$1C come from reSID as on the chip. */

/* --- input ($D100) ------------------------------------------------------ */
/* Keyboard: a FIFO of key-down events. Printable keys arrive as ASCII
 * ($20-$7E, already shifted/dead-keyed by the host layout on the desktop);
 * control keys as ASCII controls; everything else as $80+ codes. */
#define IO_KBD         (IO_INPUT + 0x00)  /* read: next event, pops; 0 if none */
#define IO_KBDST       (IO_INPUT + 0x01)  /* bit7 event available; bit0 shift, bit1 ctrl, bit2 alt held */
#define KEY_ENTER 0x0D
#define KEY_BS    0x08
#define KEY_TAB   0x09
#define KEY_ESC   0x1B
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_HOME  0x84
#define KEY_END   0x85
#define KEY_PGUP  0x86
#define KEY_PGDN  0x87
#define KEY_INS   0x88
#define KEY_DEL   0x89
#define KEY_F1    0x90                    /* F1..F12 = $90..$9B */

/* --- storage ($D300): the host filesystem, sandboxed to one directory --- */
/* Names are NUL-terminated, at NAMEPTR. Transfers go straight to RAM. */
#define IO_FS_CMD      (IO_STORAGE + 0x00) /* write: command; read: 0 idle */
#define IO_FS_STATUS   (IO_STORAGE + 0x01) /* 0 ok, 1 not found, 2 io error, 3 bad cmd, 4 end of dir, 5 name too long */
#define IO_FS_NAMEPTR  (IO_STORAGE + 0x04) /* 28-bit */
#define IO_FS_ADDR     (IO_STORAGE + 0x08) /* 28-bit RAM address for READ/WRITE/DIRNEXT */
#define IO_FS_LEN      (IO_STORAGE + 0x0C) /* 32-bit: bytes requested; updated to bytes done */
#define IO_FS_SIZE     (IO_STORAGE + 0x10) /* 32-bit: file size after OPEN/STAT */
#define FS_OPEN_READ   1   /* open NAMEPTR for reading; SIZE = file size; offset = 0 */
#define FS_OPEN_WRITE  2   /* create/truncate NAMEPTR for writing */
#define FS_READ        3   /* read LEN bytes at the current offset into ADDR; LEN = bytes read */
#define FS_WRITE       4   /* write LEN bytes from ADDR */
#define FS_CLOSE       5
#define FS_DIR_FIRST   6   /* start a directory listing */
#define FS_DIR_NEXT    7   /* copy next entry name (NUL-terminated) to ADDR, SIZE = its size; status 4 at end */
#define FS_STAT        8   /* SIZE = size of NAMEPTR, status 1 if absent */
#define FS_LOAD        9   /* convenience: OPEN_READ + read whole file to ADDR + CLOSE; LEN = size */
#define FS_SAVE       10   /* convenience: OPEN_WRITE + write LEN bytes from ADDR + CLOSE */

void    fs_set_root(const char *dir);

/* --- DMA ($D200) -------------------------------------------------------- */
/* All addresses physical, 28-bit, little-endian. Transfers are instant (§0.5). */
#define IO_DMA_SRC     (IO_DMA + 0x00)    /* 4 bytes */
#define IO_DMA_DST     (IO_DMA + 0x04)    /* 4 bytes */
#define IO_DMA_LEN     (IO_DMA + 0x08)    /* 4 bytes, bytes to move */
#define IO_DMA_CMD     (IO_DMA + 0x0C)    /* write: 1 copy, 2 fill (value = SRC byte 0), 3 swap; read: 0 = idle */
#define IO_DMA_STATUS  (IO_DMA + 0x0D)    /* read: last command, or $FF if bad */

/* --- boot-time data the frontend places in RAM (until the system ROM carries it) --- */
#define K4510_FONT8_PHYS   0x00010000u   /* 256 glyphs x 8 rows, ASCII order, 2 KB at 64 KB */
#define K4510_SCREEN_PHYS  0x00000800u   /* text map the ROM uses: 80x60 bytes */

uint8_t io_read(uint16_t addr);
void    io_frame_tick(void);                 /* called by VICKe at vblank */
void    io_write(uint16_t addr, uint8_t v);
void    io_reset(void);

void    kbd_push(uint8_t code);
void    kbd_modifiers(uint8_t shift, uint8_t ctrl, uint8_t alt);

#endif
