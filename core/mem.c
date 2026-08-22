/* K4510 memory system: 256 MB, 28-bit MAP, the eleven CPU callbacks.
 *
 * MAP semantics follow the 4510 as extended by the 45GS02, read from
 * Xemu's MEGA65 target (memory_mapper.c) rather than from a datasheet:
 *
 *   MAP with A,X,Y,Z:
 *     X == $0F : A is the megabyte for the lower 32 KB   (45GS02)
 *     else     : offset_low  = A<<8 | (X&15)<<16 ; mask[3:0] = X>>4
 *     Z == $0F : Y is the megabyte for the upper 32 KB   (45GS02)
 *     else     : offset_high = Y<<8 | (Z&15)<<16 ; mask[7:4] = Z>>4
 *   A mapped 8 KB block n (CPU $n000*2) resolves to
 *     phys = megabyte + ((offset + cpu_addr) & $FFFFF)
 *   An unmapped block resolves to phys = cpu_addr.
 *   MAP inhibits interrupts until the next EOM (NOP).
 */
#include "xemu/emutools_basicdefs.h"
#include "xemu/cpu65.h"
#include "mem.h"
#include "host.h"
#include "io.h"
#include <string.h>

uint8_t *k4510_ram;
int cpu_mega65_opcodes = 1;          /* MEGA65-build global: enable Q / 32-bit forms */

static k4510_map_t map;
static uint32_t block_base[8];       /* per 8 KB block: phys address of CPU offset 0 within it, or
                                        UNMAPPED */
#define UNMAPPED 0xFFFFFFFFu

/* ---- physical RAM ----------------------------------------------------- */

int mem_init(void)
{
    if (!k4510_ram) {
        k4510_ram = host_alloc_zeroed(K4510_PHYS_SIZE);
        if (!k4510_ram) return -1;
    } else {
        host_zero(k4510_ram, K4510_PHYS_SIZE);
    }
    mem_reset();
    return 0;
}

static void map_apply(void)
{
    for (int b = 0; b < 8; b++) {
        if (map.mask & (1u << b)) {
            uint32_t off = (b < 4) ? map.offset_low : map.offset_high;
            uint32_t mb  = (b < 4) ? map.mb_low     : map.mb_high;
            /* phys(cpu) = mb + ((off + cpu) & 0xFFFFF); cpu = b*8K + i, so precompute for i = 0 */
            block_base[b] = mb + ((off + (uint32_t)(b << 13)) & 0xFFFFFu);
        } else {
            block_base[b] = UNMAPPED;
        }
    }
}

void mem_reset(void)
{
    memset(&map, 0, sizeof map);
    map_apply();
    io_reset();
}

void mem_load(uint32_t phys, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        k4510_ram[(phys + i) & K4510_PHYS_MASK] = data[i];
}

uint8_t mem_peek(uint32_t phys)            { return k4510_ram[phys & K4510_PHYS_MASK]; }
void    mem_poke(uint32_t phys, uint8_t v) { k4510_ram[phys & K4510_PHYS_MASK] = v; }

uint32_t mem_rom_base = 0xE000;

int mem_load_rom(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t buf[K4510_ROM_MAX];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    memcpy(&k4510_ram[0x10000 - n], buf, n);
    mem_rom_base = 0x10000 - n;
    return (int)n;
}

const k4510_map_t *mem_map_state(void) { return &map; }

/* ---- CPU 16-bit view -> physical -------------------------------------- */

static XEMU_INLINE uint32_t cpu_to_phys(uint16_t a)
{
    uint32_t base = block_base[a >> 13];
    if (base == UNMAPPED) return a;
    /* within the mapped block the 20-bit add can wrap at the megabyte: redo it exactly */
    uint32_t off = (a < 0x8000) ? map.offset_low : map.offset_high;
    uint32_t mb  = (a < 0x8000) ? map.mb_low     : map.mb_high;
    (void)base;
    return (mb + ((off + a) & 0xFFFFFu)) & K4510_PHYS_MASK;
}

uint32_t mem_cpu_to_phys(uint16_t a) { return cpu_to_phys(a); }

/* ---- the eleven callbacks --------------------------------------------- */
/* I/O and ROM live in the *unmapped* view only: if a block is MAPped over
 * $D000 or $F000 the CPU sees RAM there, as on the C65/MEGA65. */

Uint8 cpu65_read_callback(Uint16 addr)
{
    uint32_t base = block_base[addr >> 13];
    if (XEMU_LIKELY(base == UNMAPPED)) {
        if (XEMU_UNLIKELY((addr & 0xF000) == K4510_IO_PAGE)) return io_read(addr);
        return k4510_ram[addr];
    }
    return k4510_ram[cpu_to_phys(addr)];
}

void cpu65_write_callback(Uint16 addr, Uint8 data)
{
    uint32_t base = block_base[addr >> 13];
    if (XEMU_LIKELY(base == UNMAPPED)) {
        if (XEMU_UNLIKELY((addr & 0xF000) == K4510_IO_PAGE)) { io_write(addr, data); return; }
        if (XEMU_UNLIKELY(addr >= mem_rom_base)) return;               /* ROM (I/O page wins: the hole in a big ROM) */
        k4510_ram[addr] = data;
        return;
    }
    k4510_ram[cpu_to_phys(addr)] = data;
}

void cpu65_write_rmw_callback(Uint16 addr, Uint8 old_data, Uint8 new_data)
{
    (void)old_data;
    cpu65_write_callback(addr, new_data);
}

/* 32-bit flat forms. The target fetches the operand byte itself (as Xemu's
 * MEGA65 target does); the base-page pointer is read through the CPU view
 * so a MAPped base page works. */
static Uint32 flat_address(Uint8 index)
{
    const Uint8  bp   = cpu65_read_callback(cpu65.pc++);
    const Uint16 base = cpu65.bphi;
    Uint32 p =  (Uint32)cpu65_read_callback(base |  bp              )
             | ((Uint32)cpu65_read_callback(base | ((bp + 1) & 0xFF)) << 8)
             | ((Uint32)cpu65_read_callback(base | ((bp + 2) & 0xFF)) << 16)
             | ((Uint32)cpu65_read_callback(base | ((bp + 3) & 0xFF)) << 24);
    return (p + index) & K4510_PHYS_MASK;
}

Uint8  cpu65_read_linear_opcode_callback(void)             { return k4510_ram[flat_address(cpu65.z)]; }
void   cpu65_write_linear_opcode_callback(Uint8 data)      { k4510_ram[flat_address(cpu65.z)] = data; }

Uint32 cpu65_read_linear_long_opcode_callback(const Uint8 index)
{
    Uint32 a = flat_address(index);
    return  (Uint32)k4510_ram[(a    ) & K4510_PHYS_MASK]
         | ((Uint32)k4510_ram[(a + 1) & K4510_PHYS_MASK] << 8)
         | ((Uint32)k4510_ram[(a + 2) & K4510_PHYS_MASK] << 16)
         | ((Uint32)k4510_ram[(a + 3) & K4510_PHYS_MASK] << 24);
}

void cpu65_write_linear_long_opcode_callback(const Uint8 index, Uint32 data)
{
    Uint32 a = flat_address(index);
    k4510_ram[(a    ) & K4510_PHYS_MASK] =  data        & 0xFF;
    k4510_ram[(a + 1) & K4510_PHYS_MASK] = (data >>  8) & 0xFF;
    k4510_ram[(a + 2) & K4510_PHYS_MASK] = (data >> 16) & 0xFF;
    k4510_ram[(a + 3) & K4510_PHYS_MASK] = (data >> 24) & 0xFF;
}

/* ---- MAP / EOM --------------------------------------------------------- */

void cpu65_do_aug_callback(void)
{
    cpu65.cpu_inhibit_interrupts = 1;       /* until EOM */
    if (cpu65.x == 0x0F) {
        map.mb_low = (uint32_t)cpu65.a << 20;
    } else {
        map.offset_low = ((uint32_t)cpu65.a << 8) | ((uint32_t)(cpu65.x & 15) << 16);
        map.mask = (map.mask & 0xF0) | (cpu65.x >> 4);
    }
    if (cpu65.z == 0x0F) {
        map.mb_high = (uint32_t)cpu65.y << 20;
    } else {
        map.offset_high = ((uint32_t)cpu65.y << 8) | ((uint32_t)(cpu65.z & 15) << 16);
        map.mask = (map.mask & 0x0F) | (cpu65.z & 0xF0);
    }
    map_apply();
}

void cpu65_do_nop_callback(void)
{
    cpu65.cpu_inhibit_interrupts = 0;       /* EOM */
}

void cpu65_illegal_opcode_callback(void)
{
    DEBUGPRINT("K4510: illegal opcode $%02X at PC=$%04X" NL, cpu65.op, cpu65.old_pc);
}
