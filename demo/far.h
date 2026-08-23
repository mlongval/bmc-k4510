/* far.h -- K-04: far memory, bank registers and far calls for cc65 programs
 * on the BMC-K4510. Everything here is a thin wrapper over hardware that
 * assembly programs use directly: the 45GS02 flat forms ([ptr],Z, in
 * prg0.s), the DMA engine at $D200, the bank registers at $D600 (K-01) and
 * the far-call gate at $DF00 (K-02). See core/io.h for the registers.
 *
 *   far_peek/poke/peek16/poke16/peek32/poke32   one value anywhere in 256 MB
 *   far_copy / far_fill                          DMA, instant, overlap-safe
 *   bank_set(block, phys) / bank_off / bank_get  8 KB block n of the CPU view
 *   far_table(desc) ; FAR_FN(n, type)            far calls through the gate
 *
 * A far descriptor is 8 bytes: {phys base, block, flags, entry}. FAR_FN(n, T)
 * is slot n of the gate as a function pointer of type T: call it like any
 * function; A/X (cc65's __fastcall__ argument and return) pass through.
 * The CPU window a program can bank freely is blocks 1-2 ($2000-$5FFF): the
 * ROM keeps its state in block 0 and the program itself is in 3-4. */
#ifndef K4510_FAR_H
#define K4510_FAR_H
#include <stdint.h>

#define FAR_REG(a) (*(volatile uint8_t *)(a))
#define FAR_DMA    0xD200u
#define FAR_BANK   0xD600u
#define FAR_GATE   0xDF00u
#define FAR_TAB    0xDF80u
#define FAR_DEPTH  0xDF84u
#define FAR_ERR    0xDF85u

/* prg0.s: native 45GS02 flat forms */
void __fastcall__ far_poke(unsigned long a, unsigned char v);
void __fastcall__ far_poke16(unsigned long a, unsigned int v);
unsigned char __fastcall__ far_peek(unsigned long a);

typedef struct { uint32_t base; uint8_t block; uint8_t flags; uint16_t entry; } far_desc_t;
#define FAR_LEAVE_BANKED 1      /* flags: do not restore the block on return */
#define FAR_NOBANK       2      /* flags: code is resident; just call it */

static void far_w32(uint16_t r, uint32_t v) { FAR_REG(r) = v; FAR_REG(r + 1) = v >> 8; FAR_REG(r + 2) = v >> 16; FAR_REG(r + 3) = v >> 24; }
static uint32_t far_r32(uint16_t r) { return (uint32_t)FAR_REG(r) | ((uint32_t)FAR_REG(r + 1) << 8) | ((uint32_t)FAR_REG(r + 2) << 16) | ((uint32_t)FAR_REG(r + 3) << 24); }

static uint16_t far_peek16(uint32_t a) { return far_peek(a) | ((uint16_t)far_peek(a + 1) << 8); }
static uint32_t far_peek32(uint32_t a) { return far_peek16(a) | ((uint32_t)far_peek16(a + 2) << 16); }
static void far_poke32(uint32_t a, uint32_t v) { far_poke16(a, (uint16_t)v); far_poke16(a + 2, (uint16_t)(v >> 16)); }

/* DMA: physical addresses; a CPU address below $10000 is its own physical address when unmapped */
static void far_copy(uint32_t dst, uint32_t src, uint32_t len) { far_w32(FAR_DMA, src); far_w32(FAR_DMA + 4, dst); far_w32(FAR_DMA + 8, len); FAR_REG(FAR_DMA + 12) = 1; }
static void far_fill(uint32_t dst, uint32_t len, uint8_t v) { FAR_REG(FAR_DMA) = v; far_w32(FAR_DMA + 4, dst); far_w32(FAR_DMA + 8, len); FAR_REG(FAR_DMA + 12) = 2; }

/* bank registers (K-01) */
static void bank_set(uint8_t block, uint32_t phys) { far_w32(FAR_BANK + 4 * block, phys & 0x0FFFFFFFUL); }
static void bank_off(uint8_t block) { FAR_REG(FAR_BANK + 4 * block + 3) = 0xFF; }
static uint32_t bank_get(uint8_t block) { return far_r32(FAR_BANK + 4 * block); }   /* 0xFFFFFFFF = off */
#define BANK_WINDOW(block) ((uint8_t *)((uint16_t)(block) << 13))               /* the CPU address of block n */
/* RAM under the ROM (K-05): bank blocks 5 and 7 onto the physical RAM at
 * $A000/$E000; the ROM stays reachable through the stub page $FF00, so every
 * ROM call keeps working. $C000-$CFFF stays ROM (next to I/O). */
static void rom_out(void) { bank_set(5, 0xA000UL); bank_set(7, 0xE000UL); }
static void rom_in(void)  { bank_off(5); bank_off(7); }

/* far calls (K-02) */
static void far_table(const far_desc_t *tab) { far_w32(FAR_TAB, (uint16_t)tab); }
#define FAR_FN(slot, type) ((type)(FAR_GATE + 4 * (slot)))
#define far_depth() FAR_REG(FAR_DEPTH)
#define far_error() FAR_REG(FAR_ERR)

#endif
