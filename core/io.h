/* K4510 I/O page: $D000-$DFFF in the unmapped CPU view.
 *
 * Layout is the PLAN-v2 proposal, PROVISIONAL until Doc signs it off;
 * every device is reached through its base constant so moving one is a
 * one-line change.
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
#define IO_SYS         0xD500u   /* $D500-$D5FF  RTC, timers, IRQ        */

/* --- input ($D100) ------------------------------------------------------ */
#define IO_KBD         (IO_INPUT + 0x00)  /* read: last key | $80; clears ready (Wozmon-shaped for now) */
#define IO_KBDCR       (IO_INPUT + 0x01)  /* bit 7: key ready */

/* --- DMA ($D200) -------------------------------------------------------- */
/* All addresses physical, 28-bit, little-endian. Transfers are instant (§0.5). */
#define IO_DMA_SRC     (IO_DMA + 0x00)    /* 4 bytes */
#define IO_DMA_DST     (IO_DMA + 0x04)    /* 4 bytes */
#define IO_DMA_LEN     (IO_DMA + 0x08)    /* 4 bytes, bytes to move */
#define IO_DMA_CMD     (IO_DMA + 0x0C)    /* write: 1 copy, 2 fill (value = SRC byte 0), 3 swap; read: 0 = idle */
#define IO_DMA_STATUS  (IO_DMA + 0x0D)    /* read: last command, or $FF if bad */

uint8_t io_read(uint16_t addr);
void    io_write(uint16_t addr, uint8_t v);
void    io_reset(void);

void    kbd_push(uint8_t ascii);

#endif
