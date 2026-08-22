/* K4510 memory system.
 *
 * 256 MB physical (28-bit), reached from the CPU's 16-bit bus through the
 * 4510/45GS02 MAP mechanism, and from the 45GS02's 32-bit flat forms
 * directly. The CPU never sees any of this: it only talks through the
 * callbacks in mem.c.
 *
 * Physical RAM is a single mmap'd region with lazy commit: the address
 * space is reserved, pages cost host memory only when first touched.
 */
#ifndef K4510_MEM_H
#define K4510_MEM_H
#include <stdint.h>
#include <stddef.h>

#define K4510_PHYS_BITS  28
#define K4510_PHYS_SIZE  (1u << K4510_PHYS_BITS)        /* 256 MB */
#define K4510_PHYS_MASK  (K4510_PHYS_SIZE - 1)

extern uint8_t *k4510_ram;          /* K4510_PHYS_SIZE bytes, lazily committed */

/* ---- spike I/O and ROM, in the CPU's unmapped 64 KB view ------------- */
#define K4510_ROM_BASE   0xF000u     /* 4 KB ROM, write-protected */
#define K4510_IO_PAGE    0xD000u     /* $D000-$DFFF: I/O */
#define K4510_IO_KBD     0xD010u     /* read: last key | $80; clears ready */
#define K4510_IO_KBDCR   0xD011u     /* bit 7: key ready */

int      mem_init(void);                                /* 0 on success */
void     mem_reset(void);                               /* MAP off, etc. */
void     mem_load(uint32_t phys, const uint8_t *data, size_t len);
uint8_t  mem_peek(uint32_t phys);                       /* physical, no side effects */
void     mem_poke(uint32_t phys, uint8_t v);
int      mem_load_rom(const char *path);                /* 4 KB into phys $F000 */

/* CPU-view translation, for tests and the monitor. */
uint32_t mem_cpu_to_phys(uint16_t cpu_addr);

/* MAP state, for tests/monitor/snapshots. */
typedef struct {
    uint32_t offset_low, offset_high;     /* 20-bit, bits 8-19 used */
    uint32_t mb_low, mb_high;             /* megabyte, already << 20 */
    uint8_t  mask;                        /* bit n: 8 KB block n is mapped */
} k4510_map_t;
const k4510_map_t *mem_map_state(void);

void     kbd_push(uint8_t ascii);

#endif
