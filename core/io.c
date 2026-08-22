#include "io.h"
#include "mem.h"
#include "vicke.h"
#include "sid.h"

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
#include <string.h>

/* ---- keyboard: a FIFO behind two registers (Wozmon polls them) -------- */
static uint8_t kbd_fifo[64];
static int     kbd_head, kbd_tail;
static uint8_t kbd_last;

void kbd_push(uint8_t ascii)
{
    int next = (kbd_tail + 1) & 63;
    if (next == kbd_head) return;
    kbd_fifo[kbd_tail] = ascii;
    kbd_tail = next;
}
static uint8_t kbd_mods;
void kbd_modifiers(uint8_t sh, uint8_t ct, uint8_t al) { kbd_mods = (sh ? 1 : 0) | (ct ? 2 : 0) | (al ? 4 : 0); }
static int kbd_ready(void) { return kbd_head != kbd_tail; }
static uint8_t kbd_read(void)
{
    if (!kbd_ready()) return 0;
    kbd_last = kbd_fifo[kbd_head]; kbd_head = (kbd_head + 1) & 63;
    return kbd_last;
}

/* ---- host filesystem -------------------------------------------------- */
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
static char fs_root[512] = "fs";
static uint8_t fs_reg[0x14];
static FILE *fs_file; static DIR *fs_dir;
void fs_set_root(const char *d) { snprintf(fs_root, sizeof fs_root, "%s", d); }
static uint32_t fs_rd32(int off) { return rd32(&fs_reg[off]); }
static void fs_wr32(int off, uint32_t v) { for (int i = 0; i < 4; i++) fs_reg[off + i] = (v >> (8 * i)) & 0xFF; }
static int fs_path(char *out, size_t max)
{
    uint32_t p = fs_rd32(4) & K4510_PHYS_MASK; char name[128]; size_t n = 0;
    for (; n < sizeof name - 1; n++) { name[n] = k4510_ram[(p + n) & K4510_PHYS_MASK]; if (!name[n]) break; }
    if (n >= sizeof name - 1) return 5;
    name[n] = 0;
    for (size_t i = 0; i < n; i++) if (name[i] == '/' || name[i] == '\\') name[i] = '_';   /* sandbox: flat names */
    if (name[0] == '.' ) return 1;
    snprintf(out, max, "%s/%s", fs_root, name);
    return 0;
}
static void fs_run(uint8_t cmd)
{
    char path[768]; int st = 0;
    uint32_t addr = fs_rd32(8) & K4510_PHYS_MASK, len = fs_rd32(12);
    switch (cmd) {
    case FS_OPEN_READ: case FS_OPEN_WRITE: case FS_STAT: case FS_LOAD: case FS_SAVE: {
        if ((st = fs_path(path, sizeof path))) break;
        if (cmd == FS_STAT) { struct stat sb; if (stat(path, &sb)) st = 1; else fs_wr32(0x10, (uint32_t)sb.st_size); break; }
        if (fs_file) { fclose(fs_file); fs_file = NULL; }
        fs_file = fopen(path, (cmd == FS_OPEN_WRITE || cmd == FS_SAVE) ? "wb" : "rb");
        if (!fs_file) { st = 1; break; }
        if (cmd == FS_OPEN_READ || cmd == FS_LOAD) { fseek(fs_file, 0, SEEK_END); long sz = ftell(fs_file); fseek(fs_file, 0, SEEK_SET); fs_wr32(0x10, (uint32_t)sz); }
        if (cmd == FS_LOAD)  { uint32_t done = 0; int c; while ((c = fgetc(fs_file)) != EOF && done < K4510_PHYS_SIZE) k4510_ram[(addr + done++) & K4510_PHYS_MASK] = (uint8_t)c; fs_wr32(12, done); fclose(fs_file); fs_file = NULL; }
        if (cmd == FS_SAVE)  { for (uint32_t i = 0; i < len; i++) fputc(k4510_ram[(addr + i) & K4510_PHYS_MASK], fs_file); fclose(fs_file); fs_file = NULL; }
        break; }
    case FS_READ: { if (!fs_file) { st = 2; break; } uint32_t done = 0; int c; while (done < len && (c = fgetc(fs_file)) != EOF) k4510_ram[(addr + done++) & K4510_PHYS_MASK] = (uint8_t)c; fs_wr32(12, done); break; }
    case FS_WRITE: { if (!fs_file) { st = 2; break; } for (uint32_t i = 0; i < len; i++) fputc(k4510_ram[(addr + i) & K4510_PHYS_MASK], fs_file); break; }
    case FS_CLOSE: if (fs_file) { fclose(fs_file); fs_file = NULL; } break;
    case FS_DIR_FIRST: if (fs_dir) closedir(fs_dir); fs_dir = opendir(fs_root); if (!fs_dir) st = 2; break;
    case FS_DIR_NEXT: {
        if (!fs_dir) { st = 2; break; }
        struct dirent *e; struct stat sb;
        while ((e = readdir(fs_dir)) && e->d_name[0] == '.') ;
        if (!e) { st = 4; closedir(fs_dir); fs_dir = NULL; break; }
        size_t i = 0; for (; e->d_name[i] && i < 63; i++) k4510_ram[(addr + i) & K4510_PHYS_MASK] = (uint8_t)e->d_name[i];
        k4510_ram[(addr + i) & K4510_PHYS_MASK] = 0;
        snprintf(path, sizeof path, "%s/%s", fs_root, e->d_name);
        fs_wr32(0x10, stat(path, &sb) ? 0 : (uint32_t)sb.st_size);
        break; }
    default: st = 3;
    }
    fs_reg[1] = (uint8_t)st; fs_reg[0] = 0;
}

/* ---- DMA ---------------------------------------------------------------- */
static uint8_t dma_reg[16];     /* SRC[4] DST[4] LEN[4] CMD STATUS .. */



static void dma_run(uint8_t cmd)
{
    uint32_t src = rd32(&dma_reg[0]) & K4510_PHYS_MASK;
    uint32_t dst = rd32(&dma_reg[4]) & K4510_PHYS_MASK;
    uint32_t len = rd32(&dma_reg[8]) & K4510_PHYS_MASK;
    dma_reg[13] = cmd;
    switch (cmd) {
    case 1:   /* copy, memmove semantics (overlap-safe), wraps at 256 MB */
        if (src + len <= K4510_PHYS_SIZE && dst + len <= K4510_PHYS_SIZE) {
            memmove(k4510_ram + dst, k4510_ram + src, len);
        } else {
            for (uint32_t i = 0; i < len; i++)       /* rare: wrap */
                k4510_ram[(dst + i) & K4510_PHYS_MASK] = k4510_ram[(src + i) & K4510_PHYS_MASK];
        }
        break;
    case 2:   /* fill with SRC byte 0 */
        if (dst + len <= K4510_PHYS_SIZE) memset(k4510_ram + dst, dma_reg[0], len);
        else for (uint32_t i = 0; i < len; i++) k4510_ram[(dst + i) & K4510_PHYS_MASK] = dma_reg[0];
        break;
    case 3:   /* swap */
        for (uint32_t i = 0; i < len; i++) {
            uint8_t *a = &k4510_ram[(src + i) & K4510_PHYS_MASK], *b = &k4510_ram[(dst + i) & K4510_PHYS_MASK];
            uint8_t t = *a; *a = *b; *b = t;
        }
        break;
    default:
        dma_reg[13] = 0xFF;
    }
    dma_reg[12] = 0;   /* instant: idle again before the CPU sees the next instruction */
}

/* ---- dispatch ------------------------------------------------------------ */

/* ---- SYS $D500 ---------------------------------------------------------- */
#include <time.h>
extern uint32_t mem_rom_base;
static uint8_t  sys_reg[0x10];
static uint32_t sys_frames;
static uint8_t  sid_shadow[4][32];
static const char sys_version[16] = "k4510 0.3";
void io_frame_tick(void) { sys_frames++; }
static void sys_latch(void)
{
    time_t t = time(NULL); struct tm *m = localtime(&t);
    sys_reg[0] = 40500 & 0xFF; sys_reg[1] = 40500 >> 8;
    sys_reg[2] = 256 & 0xFF;   sys_reg[3] = 256 >> 8;
    sys_reg[5] = m->tm_sec; sys_reg[6] = m->tm_min; sys_reg[7] = m->tm_hour;
    sys_reg[8] = m->tm_mday; sys_reg[9] = m->tm_mon + 1;
    sys_reg[10] = (m->tm_year + 1900) & 0xFF; sys_reg[11] = (m->tm_year + 1900) >> 8;
    sys_reg[12] = m->tm_wday;
}
static uint8_t sys_read(uint8_t r)
{
    if (r == 4) { sys_latch(); return 0; }
    if (r < 4) { sys_latch(); return sys_reg[r]; }
    if (r < 0x0D) return sys_reg[r];
    if (r < 0x10) return (uint8_t)(sys_frames >> ((r - 0x0D) * 8));
    if (r < 0x20) return (uint8_t)sys_version[r - 0x10];
    if (r == 0x20) return (uint8_t)(mem_rom_base >> 8);
    return 0xFF;
}

void io_reset(void)
{
    sys_frames = 0; memset(sid_shadow, 0, sizeof sid_shadow);
    kbd_head = kbd_tail = 0; kbd_last = 0;
    memset(dma_reg, 0, sizeof dma_reg);
    vicke_reset();
    sid_reset();
}

uint8_t io_read(uint16_t addr)
{
    switch (addr & 0xFF00) {
    case IO_VICKE:
        return vicke_read(addr & 0xFF);
    case IO_SID:
        if (addr < IO_FM) return ((addr & 0x1F) < 0x19) ? sid_shadow[(addr - IO_SID) >> 5][addr & 0x1F]
                                                        : sid_read((addr - IO_SID) >> 5, addr & 0x1F);
        return 0xFF;
    case IO_SYS:
        return sys_read(addr & 0xFF);
    case IO_INPUT:
        if (addr == IO_KBD)   return kbd_read();
        if (addr == IO_KBDST) return (kbd_ready() ? 0x80 : 0x00) | kbd_mods;
        return 0xFF;
    case IO_STORAGE:
        if ((addr & 0xFF) < sizeof fs_reg) return fs_reg[addr & 0xFF];
        return 0xFF;
    case IO_DMA:
        if ((addr & 0xFF) < 16) return dma_reg[addr & 0xFF];
        return 0xFF;
    default:
        return 0xFF;
    }
}

void io_write(uint16_t addr, uint8_t v)
{
    switch (addr & 0xFF00) {
    case IO_VICKE:
        vicke_write(addr & 0xFF, v); return;
    case IO_SID:
        if (addr < IO_FM) { sid_shadow[(addr - IO_SID) >> 5][addr & 0x1F] = v; sid_write((addr - IO_SID) >> 5, addr & 0x1F, v); }
        return;
    case IO_DMA:
        if ((addr & 0xFF) < 12) { dma_reg[addr & 0xFF] = v; return; }
        if (addr == IO_DMA_CMD) { dma_run(v); return; }
        return;
    case IO_STORAGE:
        if (addr == IO_FS_CMD) { fs_run(v); return; }
        if ((addr & 0xFF) < sizeof fs_reg) fs_reg[addr & 0xFF] = v;
        return;
    default:
        return;
    }
}
