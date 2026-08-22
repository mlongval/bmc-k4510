/* K4510 memory system -- spike version.
 *
 * 64 KB flat for now. Grows to 256 MB + MAP in Phase 3; the CPU never
 * sees that change because it only ever talks through the callbacks
 * in mem.c.
 */
#ifndef K4510_MEM_H
#define K4510_MEM_H
#include <stdint.h>
#include <stddef.h>

#define K4510_RAM_SIZE  0x10000u

extern uint8_t k4510_ram[K4510_RAM_SIZE];

void     mem_init(void);
void     mem_load(uint32_t addr, const uint8_t *data, size_t len);
uint8_t  mem_peek(uint32_t addr);          /* side-effect-free, for tests/monitor */
void     mem_poke(uint32_t addr, uint8_t v);

/* Spike I/O: ROM at $F000-$FFFF (write-protected), keyboard at $D010/11. */
#define K4510_ROM_BASE   0xF000u
#define K4510_IO_KBD     0xD010u    /* read: last key | $80; clears ready */
#define K4510_IO_KBDCR   0xD011u    /* bit 7: key ready */

int      mem_load_rom(const char *path);     /* 4 KB into $F000 */
void     kbd_push(uint8_t ascii);            /* from the frontend */

#endif
