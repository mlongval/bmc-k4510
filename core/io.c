#include "io.h"
#include "mem.h"
#include "xemu/emutools_basicdefs.h"
#include "xemu/cpu65.h"
static void dbg_key(uint8_t k);
static uint32_t sys_frames;
static int dbg_num;
static int dbg_auto; static uint32_t dbg_auto_next;
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
    dbg_key(ascii);
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
static char fs_cwd[256] = "";            /* relative to fs_root, no leading/trailing slash; "" = root */
static uint8_t fs_reg[0x14];
static FILE *fs_file;
void fs_set_root(const char *d) { snprintf(fs_root, sizeof fs_root, "%s", d); fs_cwd[0] = 0; }
static uint32_t fs_rd32(int off) { return rd32(&fs_reg[off]); }
static void fs_wr32(int off, uint32_t v) { for (int i = 0; i < 4; i++) fs_reg[off + i] = (v >> (8 * i)) & 0xFF; }
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
/* Resolve a guest name against the cwd inside the sandbox: "/" is the root,
 * "." and ".." work, ".." never climbs above the root. rel gets the
 * root-relative path ("" for the root), out the host path. */
static int fs_resolve(const char *name, char *rel, size_t relmax, char *out, size_t outmax)
{
    char buf[512]; size_t n = 0;
    if (name[0] == '/' || name[0] == '\\') { buf[0] = 0; name++; } else snprintf(buf, sizeof buf, "%s", fs_cwd);
    n = strlen(buf);
    while (*name) {
        const char *e = name; size_t l;
        while (*e && *e != '/' && *e != '\\') e++;
        l = (size_t)(e - name);
        if (l == 0 || (l == 1 && name[0] == '.')) { /* skip */ }
        else if (l == 2 && name[0] == '.' && name[1] == '.') { while (n && buf[n - 1] != '/') n--; if (n) n--; buf[n] = 0; }
        else { if (n + l + 2 >= sizeof buf) return 5; if (n) buf[n++] = '/'; memcpy(buf + n, name, l); n += l; buf[n] = 0; }
        name = *e ? e + 1 : e;
    }
    snprintf(rel, relmax, "%s", buf);
    if (n) snprintf(out, outmax, "%s/%s", fs_root, buf); else snprintf(out, outmax, "%s", fs_root);
    return 0;
}
/* If the host path does not exist, look for a case-insensitive match of its
 * last component in its directory (the guest upper-cases names). */
static void fs_casefix(char *path, size_t max)
{
    struct stat sb; char dir[768], *base; DIR *d; struct dirent *e;
    if (!stat(path, &sb)) return;
    snprintf(dir, sizeof dir, "%s", path); base = strrchr(dir, '/'); if (!base) return;
    *base++ = 0;
    if (!(d = opendir(dir))) return;
    while ((e = readdir(d))) if (!strcasecmp(e->d_name, base)) { snprintf(path, max, "%s/%s", dir, e->d_name); break; }
    closedir(d);
}
static int fs_guest_name(char *name, size_t max)
{
    uint32_t p = fs_rd32(4) & K4510_PHYS_MASK; size_t n = 0;
    for (; n < max - 1; n++) { name[n] = k4510_ram[(p + n) & K4510_PHYS_MASK]; if (!name[n]) break; }
    if (n >= max - 1) return 5;
    name[n] = 0;
    return 0;
}
/* host path for NAMEPTR; for reads, fall back to /PRG and /BASIC when the
 * name has no directory part and is not found where we are */
static int fs_path(char *out, size_t max, int search)
{
    char name[128], rel[256]; struct stat sb; int st;
    if ((st = fs_guest_name(name, sizeof name))) return st;
    if ((st = fs_resolve(name, rel, sizeof rel, out, max))) return st;
    fs_casefix(out, max);
    if (search && stat(out, &sb) && !strchr(name, '/') && !strchr(name, '\\')) {
        static const char *dirs[] = { "PRG", "BASIC" };
        for (int i = 0; i < 2; i++) {
            char alt[128]; snprintf(alt, sizeof alt, "/%s/%s", dirs[i], name);
            if (fs_resolve(alt, rel, sizeof rel, out, max)) continue;
            fs_casefix(out, max);
            if (!stat(out, &sb)) return 0;
        }
        fs_resolve(name, rel, sizeof rel, out, max); fs_casefix(out, max);   /* not found: the plain path, for the error */
    }
    return 0;
}
/* directory listing: read, sort, serve */
static char (*fs_list)[64]; static uint32_t *fs_list_size; static int fs_list_n, fs_list_i;
static int fs_cmp(const void *a, const void *b) { return strcasecmp((const char *)a, (const char *)b); }
static int fs_dir_first(void)
{
    char path[768], rel[256]; DIR *d; struct dirent *e; int n = 0, cap = 64;
    fs_resolve("", rel, sizeof rel, path, sizeof path);
    if (!(d = opendir(path))) return 2;
    free(fs_list); free(fs_list_size); fs_list = malloc(cap * sizeof *fs_list); fs_list_size = malloc(cap * sizeof *fs_list_size);
    while ((e = readdir(d))) {
        char full[1024]; struct stat sb;
        if (e->d_name[0] == '.') continue;
        if (n == cap) { cap *= 2; fs_list = realloc(fs_list, cap * sizeof *fs_list); fs_list_size = realloc(fs_list_size, cap * sizeof *fs_list_size); }
        snprintf(fs_list[n], 64, "%s", e->d_name);
        snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        fs_list_size[n] = stat(full, &sb) ? 0 : S_ISDIR(sb.st_mode) ? 0xFFFFFFFFu : (uint32_t)sb.st_size;
        n++;
    }
    closedir(d);
    /* sort names and sizes together: sort an index */
    { int *idx = malloc(n * sizeof *idx); for (int i = 0; i < n; i++) idx[i] = i;
      for (int i = 1; i < n; i++) { int k = idx[i], j = i; while (j > 0 && strcasecmp(fs_list[idx[j - 1]], fs_list[k]) > 0) { idx[j] = idx[j - 1]; j--; } idx[j] = k; }
      char (*nl)[64] = malloc(n * sizeof *nl); uint32_t *ns = malloc(n * sizeof *ns);
      for (int i = 0; i < n; i++) { memcpy(nl[i], fs_list[idx[i]], 64); ns[i] = fs_list_size[idx[i]]; }
      free(fs_list); free(fs_list_size); fs_list = nl; fs_list_size = ns; free(idx); }
    (void)fs_cmp;
    fs_list_n = n; fs_list_i = 0;
    return 0;
}
#include <stdlib.h>
static void fs_run(uint8_t cmd)
{
    char path[768]; int st = 0;
    uint32_t addr = fs_rd32(8) & K4510_PHYS_MASK, len = fs_rd32(12);
    switch (cmd) {
    case FS_OPEN_READ: case FS_OPEN_WRITE: case FS_STAT: case FS_LOAD: case FS_SAVE: {
        int rd = (cmd == FS_OPEN_READ || cmd == FS_STAT || cmd == FS_LOAD);
        if ((st = fs_path(path, sizeof path, rd))) break;
        if (cmd == FS_STAT) { struct stat sb; if (stat(path, &sb)) st = 1; else fs_wr32(0x10, S_ISDIR(sb.st_mode) ? 0xFFFFFFFFu : (uint32_t)sb.st_size); break; }
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
    case FS_DIR_FIRST: st = fs_dir_first(); break;
    case FS_DIR_NEXT: {
        if (!fs_list) { st = 2; break; }
        if (fs_list_i >= fs_list_n) { st = 4; break; }
        size_t i = 0; const char *nm = fs_list[fs_list_i];
        for (; nm[i] && i < 63; i++) k4510_ram[(addr + i) & K4510_PHYS_MASK] = (uint8_t)nm[i];
        k4510_ram[(addr + i) & K4510_PHYS_MASK] = 0;
        fs_wr32(0x10, fs_list_size[fs_list_i]);
        fs_list_i++;
        break; }
    case FS_CHDIR: {
        char name[128], rel[256]; struct stat sb;
        if ((st = fs_guest_name(name, sizeof name))) break;
        if ((st = fs_resolve(name, rel, sizeof rel, path, sizeof path))) break;
        fs_casefix(path, sizeof path);
        if (stat(path, &sb) || !S_ISDIR(sb.st_mode)) { st = 1; break; }
        /* keep the host's spelling of the directory in the cwd */
        snprintf(fs_cwd, sizeof fs_cwd, "%s", strlen(path) > strlen(fs_root) ? path + strlen(fs_root) + 1 : "");
        break; }
    case FS_MKDIR: if ((st = fs_path(path, sizeof path, 0))) break; if (mkdir(path, 0777)) st = 2; break;
    case FS_RM:    { struct stat sb; if ((st = fs_path(path, sizeof path, 0))) break; if (stat(path, &sb)) { st = 1; break; } if (S_ISDIR(sb.st_mode) || unlink(path)) st = 2; break; }
    case FS_RMDIR: { struct stat sb; if ((st = fs_path(path, sizeof path, 0))) break; if (stat(path, &sb)) { st = 1; break; }
#ifdef K4510_PI
        st = 2;                       /* circle-syscallwrap has no rmdir yet */
#else
        if (!S_ISDIR(sb.st_mode) || rmdir(path)) st = 2;
#endif
        break; }
    case FS_GETCWD: { size_t i = 0; k4510_ram[addr & K4510_PHYS_MASK] = '/'; for (; fs_cwd[i] && i < 250; i++) k4510_ram[(addr + 1 + i) & K4510_PHYS_MASK] = (uint8_t)fs_cwd[i]; k4510_ram[(addr + 1 + i) & K4510_PHYS_MASK] = 0; fs_wr32(0x10, (uint32_t)i + 1); break; }
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
static uint8_t  sid_shadow[4][32];
static const char sys_version[16] = "k4510 0.3";
void io_frame_tick(void) { sys_frames++; if (dbg_auto && sys_frames >= dbg_auto_next) { dbg_auto_next = sys_frames + 900; dbg_dump("auto, 15 s"); } }
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
    if (r == 0xF0) return (uint8_t)dbg_num;
    if (r == 0xF2) return (uint8_t)dbg_auto;
    return 0xFF;
}

/* ---- debug recorder and DUMP ------------------------------------------- */
#define DBG_PCS 4096
#define DBG_KEYS 256
#define DBG_LOG 8192
static uint16_t dbg_pcs[DBG_PCS]; static uint32_t dbg_pci;
static uint8_t dbg_keys[DBG_KEYS]; static uint32_t dbg_keyi;
static char dbg_log[DBG_LOG]; static uint32_t dbg_logi;
void dbg_pc(uint16_t pc) { dbg_pcs[dbg_pci++ & (DBG_PCS - 1)] = pc; }
static void dbg_key(uint8_t k) { dbg_keys[dbg_keyi++ & (DBG_KEYS - 1)] = k; }
static void dbg_logc(uint8_t c) { dbg_log[dbg_logi++ & (DBG_LOG - 1)] = (char)c; }
extern uint8_t vicke_read(uint8_t r);
int dbg_dump(const char *why)
{
    char name[64]; FILE *f; time_t t = time(NULL); struct tm *m = localtime(&t);
    mkdir("dumps", 0777);
    if (++dbg_num > 100) dbg_num = 1;                  /* rotate: at most 100 files */
    snprintf(name, sizeof name, "dumps/dump-%03d.txt", dbg_num);
    if (!(f = fopen(name, "w"))) { dbg_num--; return -1; }
    fprintf(f, "BMC-K4510 dump %d  %04d-%02d-%02d %02d:%02d:%02d  (%s)\n", dbg_num, m->tm_year + 1900, m->tm_mon + 1, m->tm_mday, m->tm_hour, m->tm_min, m->tm_sec, why);
    fprintf(f, "frame %u  cwd /%s\n\n", (unsigned)sys_frames, fs_cwd);
    { uint8_t pf = cpu65_get_pf(); char fl[8]; const char *names = "NVEBDIZC";
      for (int b = 0; b < 8; b++) fl[b] = (pf & (0x80 >> b)) ? names[b] : '-';
      fprintf(f, "CPU  PC=%04X A=%02X X=%02X Y=%02X Z=%02X SP=%04X  P=%02X (%.8s)  B=%04X  inhibit=%d\n",
            cpu65.pc, cpu65.a, cpu65.x, cpu65.y, cpu65.z, cpu65.s | cpu65.sphi, pf, fl, cpu65.bphi, cpu65.cpu_inhibit_interrupts); }
    { const k4510_map_t *mp = mem_map_state();
      fprintf(f, "MAP  mask=%02X off_lo=%05X off_hi=%05X mb_lo=%X mb_hi=%X   BANKS mask=%02X", mp->mask, mp->offset_low, mp->offset_high, mp->mb_low >> 20, mp->mb_high >> 20, mem_bank_mask());
      for (int b = 0; b < 8; b++) if (mem_bank_get(b) != BANK_OFF) fprintf(f, " %d=%07X", b, mem_bank_get(b));
      fprintf(f, "   FAR table=%07X depth=%d err=%d\n", far_table, far_depth, far_err); }
    fprintf(f, "VICKe ctrl=%02X bg=%02X irqst=%02X irqmask=%02X\n", vicke_read(0), vicke_read(1), vicke_read(4), vicke_read(5));
    for (int n = 0; n < 4; n++) { fprintf(f, "  layer %d:", n); for (int i = 0; i < 16; i++) fprintf(f, " %02X", vicke_read(0x10 + n * 16 + i)); fprintf(f, "\n"); }
    fprintf(f, "  sprites=%02X sheila=%02X list=", vicke_read(0x0E), vicke_read(0x64)); for (int i = 3; i >= 0; i--) fprintf(f, "%02X", vicke_read(0x60 + i)); fprintf(f, "\n");
    for (int c = 0; c < 4; c++) { fprintf(f, "SID%d:", c); for (int i = 0; i < 25; i++) fprintf(f, " %02X", sid_shadow[c][i]); fprintf(f, "\n"); }
    fprintf(f, "FS   reg:"); for (int i = 0; i < 0x14; i++) fprintf(f, " %02X", fs_reg[i]); fprintf(f, "   DMA:"); for (int i = 0; i < 14; i++) fprintf(f, " %02X", dma_reg[i]); fprintf(f, "\n");
    fprintf(f, "MATH F0..F7:"); for (int i = 0; i < 8; i++) fprintf(f, " %g", mf_get(i)); fprintf(f, "  FI=%d flags=%02X mlstat=%02X\n", (int)m32(0x24), math_reg[0x22], math_reg[0x2D]);
    fprintf(f, "\nSCREEN (text layer at $030000, 80 columns):\n");
    for (int y = 0; y < 60; y++) { char r[81]; int last = -1; for (int x = 0; x < 80; x++) { uint8_t ch = k4510_ram[0x30000 + (y * 80 + x) * 4]; r[x] = (ch >= 0x20 && ch < 0x7F) ? ch : (ch ? '.' : ' '); if (r[x] != ' ') last = x; } r[last + 1] = 0; if (last >= 0) fprintf(f, "%2d|%s\n", y, r); }
    fprintf(f, "\nSHELL LOG (command lines and DUMP notes, oldest first):\n");
    { uint32_t n = dbg_logi < DBG_LOG ? dbg_logi : DBG_LOG, start = dbg_logi - n; for (uint32_t i = 0; i < n; i++) fputc(dbg_log[(start + i) & (DBG_LOG - 1)], f); fprintf(f, "\n"); }
    fprintf(f, "\nKEYS (last %u, oldest first, hex):", dbg_keyi < DBG_KEYS ? dbg_keyi : DBG_KEYS);
    { uint32_t n = dbg_keyi < DBG_KEYS ? dbg_keyi : DBG_KEYS, start = dbg_keyi - n; for (uint32_t i = 0; i < n; i++) { uint8_t k = dbg_keys[(start + i) & (DBG_KEYS - 1)]; if (k >= 0x20 && k < 0x7F) fprintf(f, " %c", k); else fprintf(f, " %02X", k); } fprintf(f, "\n"); }
    fprintf(f, "\nPC HISTORY (last %u opcode fetches, oldest first; runs of consecutive PCs collapsed as a-b):\n", dbg_pci < DBG_PCS ? dbg_pci : DBG_PCS);
    { uint32_t n = dbg_pci < DBG_PCS ? dbg_pci : DBG_PCS, start = dbg_pci - n; int col = 0; uint16_t run0 = 0, prev = 0; int inrun = 0;
      for (uint32_t i = 0; i <= n; i++) {
          uint16_t pc = i < n ? dbg_pcs[(start + i) & (DBG_PCS - 1)] : 0; int seq = i < n && inrun && pc > prev && pc - prev <= 3;
          if (i == 0) { run0 = pc; prev = pc; inrun = 1; continue; }
          if (seq) { prev = pc; continue; }
          if (run0 == prev) col += fprintf(f, "%04X ", run0); else col += fprintf(f, "%04X-%04X ", run0, prev);
          if (col > 90) { fprintf(f, "\n"); col = 0; }
          run0 = pc; prev = pc;
      }
      fprintf(f, "\n"); }
    fprintf(f, "\nZERO PAGE:\n"); for (int i = 0; i < 256; i += 32) { fprintf(f, "%02X:", i); for (int j = 0; j < 32; j++) fprintf(f, " %02X", k4510_ram[i + j]); fprintf(f, "\n"); }
    fprintf(f, "STACK $0100-$01FF:\n"); for (int i = 0x100; i < 0x200; i += 32) { fprintf(f, "%04X:", i); for (int j = 0; j < 32; j++) fprintf(f, " %02X", k4510_ram[i + j]); fprintf(f, "\n"); }
    fprintf(f, "$0300-$04FF (EhBASIC vectors, input buffer, K4510 glue state):\n"); for (int i = 0x300; i < 0x500; i += 32) { fprintf(f, "%04X:", i); for (int j = 0; j < 32; j++) fprintf(f, " %02X", k4510_ram[i + j]); fprintf(f, "\n"); }
    fclose(f);
    fprintf(stderr, "K4510: %s written (%s)\n", name, why);
    return dbg_num;
}

void io_reset(void)
{
    sys_frames = 0; dbg_auto = 1; dbg_auto_next = 900;   /* auto dump on by default (Doc, 2026-08-23): every 15 s, 100 files rotating */
    memset(sid_shadow, 0, sizeof sid_shadow); memset(math_reg, 0, sizeof math_reg); math_int_update();
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
    case IO_BANK: {
        uint8_t r = addr & 0xFF;
        if (r < 0x20) { uint32_t v = mem_bank_get(r >> 2) == BANK_OFF ? (mem_bank_base(r >> 2) | 0xFF000000u) : mem_bank_base(r >> 2); return (uint8_t)(v >> (8 * (r & 3))); }   /* off: base with byte 3 = $FF */
        if (r == 0x20) return mem_bank_mask();
        if (r == 0x21) return mem_map_state()->mask;
        return 0xFF;
    }
    case IO_FAR: {
        uint8_t r = addr & 0xFF;
        if (r >= 0x80 && r < 0x84) return (uint8_t)(far_table >> (8 * (r - 0x80)));
        if (r == 0x84) return far_depth;
        if (r == 0x85) return far_err;
        return 0xFF;
    }
    case IO_INPUT:
        if (addr == IO_KBD)   return kbd_read();
        if (addr == IO_KBDST) return (kbd_ready() ? 0x80 : 0x00) | kbd_mods;
        if (addr == IO_KBDST + 1) return kbd_ready() ? kbd_fifo[kbd_head] : 0;   /* peek: next key, not popped */
        if (addr == IO_KBDST + 2) {                                              /* break pending: an ESC or Ctrl-C anywhere in the queue is removed and returned */
            for (int i = kbd_head; i != kbd_tail; i = (i + 1) & 63) {
                uint8_t k = kbd_fifo[i];
                if (k == 0x03 || k == 0x1B) { for (int j = i; j != kbd_tail; j = (j + 1) & 63) kbd_fifo[j] = kbd_fifo[(j + 1) & 63]; kbd_tail = (kbd_tail + 63) & 63; return k; }
            }
            return 0;
        }
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
    case IO_SYS:
        if ((addr & 0xFF) == 0xF0) dbg_dump("DUMP register");
        if ((addr & 0xFF) == 0xF1) dbg_logc(v);
        if ((addr & 0xFF) == 0xF2) { dbg_auto = v ? 1 : 0; dbg_auto_next = sys_frames + 900; }
        return;
    case IO_BANK: {
        uint8_t r = addr & 0xFF, b = r >> 2, i = r & 3;
        if (r >= 0x20) return;
        { uint32_t cur = (mem_bank_base(b) & ~(0xFFu << (8 * i))) | ((uint32_t)v << (8 * i));
          if (i == 3) { if (v & 0x80) { mem_bank_setbase(b, cur); mem_bank_off(b); } else mem_bank_set(b, cur); }   /* byte 3 decides on/off */
          else mem_bank_setbase(b, cur); }                                                                           /* bytes 0-2: the base only */
        return;
    }
    case IO_FAR: {
        uint8_t r = addr & 0xFF;
        if (r >= 0x80 && r < 0x84) { far_table = (far_table & ~(0xFFu << (8 * (r - 0x80)))) | ((uint32_t)v << (8 * (r - 0x80))); far_table &= K4510_PHYS_MASK; }
        if (r == 0x85) far_err = 0;
        return;
    }
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
