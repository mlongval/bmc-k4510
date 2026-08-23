#include "io.h"
#include "mem.h"
#include "vicke.h"
#include "sid.h"

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
#include <string.h>
#include <stdint.h>

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

/* ---- MATH $D700 --------------------------------------------------------- */
#include <math.h>
static uint8_t math_reg[0x80];       /* $D700-$D77F image; F0..F7 at 0, FI at $24, integer unit at $68.. */
static float  mf_get(int n) { float f; memcpy(&f, &math_reg[n * 4], 4); return f; }
static void   mf_set(int n, float f) { memcpy(&math_reg[n * 4], &f, 4); }
static uint32_t m32(int off) { return (uint32_t)math_reg[off] | ((uint32_t)math_reg[off + 1] << 8) | ((uint32_t)math_reg[off + 2] << 16) | ((uint32_t)math_reg[off + 3] << 24); }
static void m32w(int off, uint32_t v) { for (int i = 0; i < 4; i++) math_reg[off + i] = (uint8_t)(v >> (8 * i)); }
static void math_int_update(void)
{
    uint32_t a = m32(0x70), b = m32(0x74);
    uint64_t p = (uint64_t)a * b;
    m32w(0x78, (uint32_t)p); m32w(0x7C, (uint32_t)(p >> 32));
    if (b == 0) { m32w(0x6C, 0xFFFFFFFFu); m32w(0x68, 0xFFFFFFFFu); }
    else { uint64_t q = ((uint64_t)a << 32) / b; m32w(0x6C, (uint32_t)(q >> 32)); m32w(0x68, (uint32_t)q); }
}
static void math_fop(uint8_t op)
{
    int d = (math_reg[0x21] >> 4) & 7, sidx = math_reg[0x21] & 7;
    float a = mf_get(d), b = mf_get(sidx), r = a; int store = 1;
    switch (op & 0x1F) {
    case MATH_MOV: r = b; break;       case MATH_ADD: r = a + b; break;   case MATH_SUB: r = a - b; break;
    case MATH_MUL: r = a * b; break;   case MATH_DIV: r = a / b; break;
    case MATH_SQRT: r = sqrtf(b); break; case MATH_SIN: r = sinf(b); break; case MATH_COS: r = cosf(b); break;
    case MATH_TAN: r = tanf(b); break; case MATH_ATAN: r = atanf(b); break; case MATH_ATAN2: r = atan2f(a, b); break;
    case MATH_EXP: r = expf(b); break; case MATH_LOG: r = logf(b); break;  case MATH_POW: r = powf(a, b); break;
    case MATH_ABS: r = fabsf(b); break; case MATH_NEG: r = -b; break;     case MATH_FLOOR: r = floorf(b); break;
    case MATH_ROUND: r = roundf(b); break; case MATH_FMOD: r = fmodf(a, b); break;
    case MATH_CMP: r = a - b; store = 0; break;
    case MATH_ITOF: r = (float)(int32_t)m32(0x24); break;
    case MATH_FTOI: { float t = truncf(b); int32_t i = (t > 2147483520.0f) ? INT32_MAX : (t < -2147483648.0f) ? INT32_MIN : (int32_t)t; m32w(0x24, (uint32_t)i); r = b; store = 0; break; }
    default: store = 0; break;
    }
    if (store) mf_set(d, r);
    math_reg[0x22] = (uint8_t)((r == 0.0f ? 1 : 0) | (r < 0.0f ? 2 : 0) | (isnan(r) || isinf(r) ? 4 : 0));
}
static void math_list_run(void)
{
    uint32_t pc = m32(0x28) & K4510_PHYS_MASK;
    uint16_t cnt = (uint16_t)(math_reg[0x2E] | (math_reg[0x2F] << 8));
    uint8_t status = 0xFF;
    for (int guard = 0; guard < 65536; guard++) {
        uint8_t op = k4510_ram[pc & K4510_PHYS_MASK], arg = k4510_ram[(pc + 1) & K4510_PHYS_MASK];
        pc += 2;
        if (op < 0x20) { math_reg[0x21] = arg; math_fop(op); continue; }
        int fl = math_reg[0x22];
        switch (op) {
        case ML_END:     status = 0; goto done;
        case ML_STOPNEG: if (fl & 2)   { status = 1; goto done; } break;
        case ML_STOPPOS: if (!(fl & 2)) { status = 1; goto done; } break;
        case ML_STOPZERO: if (fl & 1)  { status = 1; goto done; } break;
        case ML_STOPNZ:  if (!(fl & 1)) { status = 1; goto done; } break;
        case ML_JUMP:    pc += (int8_t)arg * 2; break;
        case ML_DJNZ:    if (--cnt) pc += (int8_t)arg * 2; break;
        case ML_STOPFIGE: { int32_t fi = (int32_t)m32(0x24); if (fi >= (int32_t)arg) { status = 1; goto done; } break; }
        case ML_LDF:     for (int i = 0; i < 4; i++) math_reg[((arg >> 4) & 7) * 4 + i] = k4510_ram[(pc + i) & K4510_PHYS_MASK]; pc += 4; break;
        case ML_LDI:     for (int i = 0; i < 4; i++) math_reg[0x24 + i] = k4510_ram[(pc + i) & K4510_PHYS_MASK]; pc += 4; break;
        case ML_LDMS: {  uint32_t a = ((uint32_t)k4510_ram[pc & K4510_PHYS_MASK] | ((uint32_t)k4510_ram[(pc + 1) & K4510_PHYS_MASK] << 8)
                         | ((uint32_t)k4510_ram[(pc + 2) & K4510_PHYS_MASK] << 16) | ((uint32_t)k4510_ram[(pc + 3) & K4510_PHYS_MASK] << 24)) & K4510_PHYS_MASK;
                         uint8_t e = k4510_ram[a], m1 = k4510_ram[(a + 1) & K4510_PHYS_MASK], m2 = k4510_ram[(a + 2) & K4510_PHYS_MASK], m3 = k4510_ram[(a + 3) & K4510_PHYS_MASK];
                         float f = 0.0f;
                         if (e >= 2) { uint32_t bits = ((uint32_t)(m1 & 0x80) << 24) | ((uint32_t)(e - 2) << 23) | ((uint32_t)(m1 & 0x7F) << 16) | ((uint32_t)m2 << 8) | m3; memcpy(&f, &bits, 4); }
                         mf_set((arg >> 4) & 7, f); pc += 4; break; }
        default:         status = 0xFF; goto done;
        }
    }
done:
    math_reg[0x2D] = status; math_reg[0x2E] = (uint8_t)cnt; math_reg[0x2F] = (uint8_t)(cnt >> 8);
}
static uint8_t math_read(uint8_t r) { return r < sizeof math_reg ? math_reg[r] : 0xFF; }
static void math_write(uint8_t r, uint8_t v)
{
    if (r >= sizeof math_reg) return;
    if (r == 0x20) { math_fop(v); return; }
    if (r == 0x2C) { math_list_run(); return; }
    math_reg[r] = v;
    if (r >= 0x70 && r < 0x78) math_int_update();
}

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
    sys_frames = 0; memset(sid_shadow, 0, sizeof sid_shadow); memset(math_reg, 0, sizeof math_reg); math_int_update();
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
    case IO_MATH:
        return math_read(addr & 0xFF);
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
    case IO_MATH:
        math_write(addr & 0xFF, v); return;
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
