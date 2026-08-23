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
#define K4510_ROM_MAX    0x8000u     /* up to 32 KB ROM, write-protected; files load top-aligned at $10000 */
extern uint32_t mem_rom_base;        /* first ROM address in the CPU view; set by mem_load_rom */
#define K4510_IO_PAGE    0xD000u     /* $D000-$DFFF: I/O, see io.h */

int      mem_init(void);                                /* 0 on success */
void     mem_reset(void);                               /* MAP off, etc. */
void     mem_load(uint32_t phys, const uint8_t *data, size_t len);
uint8_t  mem_peek(uint32_t phys);                       /* physical, no side effects */
void     mem_poke(uint32_t phys, uint8_t v);
int      mem_load_rom(const char *path);                /* <= 32 KB, top-aligned at $10000; returns size */

/* CPU-view translation, for tests and the monitor. */
uint32_t mem_cpu_to_phys(uint16_t cpu_addr);

/* MAP state, for tests/monitor/snapshots. */
typedef struct {
    uint32_t offset_low, offset_high;     /* 20-bit, bits 8-19 used */
    uint32_t mb_low, mb_high;             /* megabyte, already << 20 */
    uint8_t  mask;                        /* bit n: 8 KB block n is mapped */
} k4510_map_t;
const k4510_map_t *mem_map_state(void);

/* ---- bank registers (K-01) and the far-call gate (K-02) -------------- */
/* A bank register puts one 8 KB block of the CPU view onto any 28-bit
 * physical base (byte granularity): phys = base + (cpu & $1FFF). A block
 * is owned by whichever wrote it last, MAP or a bank register; MAP always
 * rewrites all eight blocks, so "MAP everything off" also clears the banks. */
#define BANK_OFF 0xFFFFFFFFu
void     mem_bank_set(uint8_t block, uint32_t phys);    /* block 0-7 */
void     mem_bank_off(uint8_t block);
uint32_t mem_bank_get(uint8_t block);                   /* BANK_OFF if not banked */
uint8_t  mem_bank_mask(void);                           /* bit n: block n banked */
/* Far-call gate: JSR $DF00+4n banks descriptor n in and jumps to it; the
 * callee's RTS lands on the return gate, which restores the bank. */
#define FAR_GATE     0xDF00u
#define FAR_SLOTS    32
#define FAR_RET      0xDFF0u
#define FAR_DEPTH_MAX 64
extern uint32_t far_table;          /* 28-bit phys of the descriptor table ($DF80-$DF83) */
extern uint8_t  far_depth, far_err; /* nesting depth; last error: 1 overflow, 2 underflow, 3 bad slot */


#endif
