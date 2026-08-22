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

#endif
