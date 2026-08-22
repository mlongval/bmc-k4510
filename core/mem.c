/* K4510 memory system -- spike version: the eleven callbacks cpu65.c needs.
 *
 * This is the whole CPU<->machine interface. Everything the 45GS02 does to
 * the outside world comes through here.
 */
#include "xemu/emutools_basicdefs.h"
#include "xemu/cpu65.h"
#include "mem.h"
#include <string.h>

uint8_t k4510_ram[K4510_RAM_SIZE];

/* ---- spike keyboard: a tiny FIFO behind two Apple-1-shaped registers --- */
static uint8_t kbd_fifo[64];
static int     kbd_head, kbd_tail;
static uint8_t kbd_last;

void kbd_push(uint8_t ascii)
{
    int next = (kbd_tail + 1) & 63;
    if (next == kbd_head) return;           /* full: drop */
    kbd_fifo[kbd_tail] = ascii;
    kbd_tail = next;
}

static int kbd_ready(void) { return kbd_head != kbd_tail; }

static uint8_t kbd_read(void)
{
    if (kbd_ready()) {
        kbd_last = kbd_fifo[kbd_head] | 0x80;   /* Wozmon wants bit 7 set */
        kbd_head = (kbd_head + 1) & 63;
    }
    return kbd_last;
}

int mem_load_rom(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(&k4510_ram[K4510_ROM_BASE], 1, K4510_RAM_SIZE - K4510_ROM_BASE, f);
    fclose(f);
    return (int)n;
}

/* MEGA65-build globals the core references. */
int cpu_mega65_opcodes = 1;     /* enable the Q / 32-bit extensions */

void mem_init(void)
{
    memset(k4510_ram, 0, sizeof k4510_ram);
}

void mem_load(uint32_t addr, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        k4510_ram[(addr + i) & (K4510_RAM_SIZE - 1)] = data[i];
}

uint8_t mem_peek(uint32_t addr)            { return k4510_ram[addr & (K4510_RAM_SIZE - 1)]; }
void    mem_poke(uint32_t addr, uint8_t v) { k4510_ram[addr & (K4510_RAM_SIZE - 1)] = v; }

/* ---- 16-bit CPU-visible bus ------------------------------------------ */

Uint8 cpu65_read_callback(Uint16 addr)
{
    if (XEMU_UNLIKELY((addr & 0xFF00) == 0xD000)) {
        switch (addr) {
        case K4510_IO_KBD:   return kbd_read();
        case K4510_IO_KBDCR: return kbd_ready() ? 0x80 : 0x00;
        default:             return 0xFF;
        }
    }
    return k4510_ram[addr];
}

void cpu65_write_callback(Uint16 addr, Uint8 data)
{
    if (XEMU_UNLIKELY(addr >= K4510_ROM_BASE)) return;        /* ROM */
    if (XEMU_UNLIKELY((addr & 0xFF00) == 0xD000)) return;     /* no writable I/O yet */
    k4510_ram[addr] = data;
}

void cpu65_write_rmw_callback(Uint16 addr, Uint8 old_data, Uint8 new_data)
{
    (void)old_data;                         /* no I/O yet that cares about the double write */
    cpu65_write_callback(addr, new_data);
}

/* ---- 32-bit flat forms: [$nn],Z and the Q-register quad forms ------- */
/* The core hands us no address and has NOT fetched the operand: the target
 * plays the CPU for this one byte, exactly as Xemu's MEGA65 target does
 * (memory_mapper.c: cpu_get_flat_addressing_mode_address). The base-page
 * byte names a 32-bit little-endian pointer; add Z (or the quad index). */

static Uint32 flat_address(Uint8 index)
{
    const Uint8  bp   = cpu65_read_callback(cpu65.pc++);  /* fetch the operand */
    const Uint16 base = cpu65.bphi;                        /* base page, already <<8 */
    Uint32 p =  (Uint32)k4510_ram[base |  bp              ]
             | ((Uint32)k4510_ram[base | ((bp + 1) & 0xFF)] << 8)
             | ((Uint32)k4510_ram[base | ((bp + 2) & 0xFF)] << 16)
             | ((Uint32)k4510_ram[base | ((bp + 3) & 0xFF)] << 24);
    return (p + index) & 0x0FFFFFFFu;       /* 28-bit space */
}

Uint8 cpu65_read_linear_opcode_callback(void)
{
    return k4510_ram[flat_address(cpu65.z) & (K4510_RAM_SIZE - 1)];
}

void cpu65_write_linear_opcode_callback(Uint8 data)
{
    k4510_ram[flat_address(cpu65.z) & (K4510_RAM_SIZE - 1)] = data;
}

Uint32 cpu65_read_linear_long_opcode_callback(const Uint8 index)
{
    Uint32 a = flat_address(index);
    return  (Uint32)k4510_ram[(a    ) & (K4510_RAM_SIZE - 1)]
         | ((Uint32)k4510_ram[(a + 1) & (K4510_RAM_SIZE - 1)] << 8)
         | ((Uint32)k4510_ram[(a + 2) & (K4510_RAM_SIZE - 1)] << 16)
         | ((Uint32)k4510_ram[(a + 3) & (K4510_RAM_SIZE - 1)] << 24);
}

void cpu65_write_linear_long_opcode_callback(const Uint8 index, Uint32 data)
{
    Uint32 a = flat_address(index);
    k4510_ram[(a    ) & (K4510_RAM_SIZE - 1)] =  data        & 0xFF;
    k4510_ram[(a + 1) & (K4510_RAM_SIZE - 1)] = (data >>  8) & 0xFF;
    k4510_ram[(a + 2) & (K4510_RAM_SIZE - 1)] = (data >> 16) & 0xFF;
    k4510_ram[(a + 3) & (K4510_RAM_SIZE - 1)] = (data >> 24) & 0xFF;
}

/* ---- MAP / EOM --------------------------------------------------------- */
/* Spike: MAP is accepted and ignored (flat 64 KB). Phase 3 gives it the
 * 28-bit MMU. EOM (NOP) re-enables interrupts after MAP, as on silicon. */

void cpu65_do_aug_callback(void)
{
    cpu65.cpu_inhibit_interrupts = 1;
}

void cpu65_do_nop_callback(void)
{
    cpu65.cpu_inhibit_interrupts = 0;
}

/* ---- misc -------------------------------------------------------------- */

void cpu65_illegal_opcode_callback(void)
{
    DEBUGPRINT("K4510: illegal opcode $%02X at PC=$%04X" NL, cpu65.op, cpu65.old_pc);
}
