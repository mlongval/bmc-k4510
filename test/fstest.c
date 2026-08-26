#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void W32(uint16_t r, uint32_t v) { for (int i = 0; i < 4; i++) io_write(r + i, (v >> (8 * i)) & 0xFF); }
static uint32_t R32(uint16_t r) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)io_read(r + i) << (8 * i); return v; }
/* The tests own their fixture: hello.txt used to be shipped in fs/, so
 * deleting it broke them. Created here, removed at the end. */
static void fixture(int make)
{
    if (make) { FILE *f = fopen("fs/hello.txt", "wb"); if (f) { fputs("hello from the host filesystem\n", f); fclose(f); } }
    else remove("fs/hello.txt");
}
int main(void)
{
    fixture(1);
    /* /STARTUP.BAT is the user's file and is gitignored, so it differs from
     * machine to machine -- one that ends in CLS wipes the banner this test
     * looks for.  Tell the ROM not to run it: the test owns its boot. */
    io_set_opts(SYSOPT_NOBOOT);
    mem_init(); fs_set_root("fs");
    mem_load(0x0300, (const uint8_t *)"hello.txt", 10);
    W32(IO_FS_NAMEPTR, 0x300); W32(IO_FS_ADDR, 0x100000); io_write(IO_FS_CMD, FS_LOAD);
    uint32_t n = R32(IO_FS_LEN);
    printf("1. LOAD hello.txt -> status %d, %u bytes: '%.*s'\n", io_read(IO_FS_STATUS), n, (int)(n > 20 ? 20 : n), (char *)&k4510_ram[0x100000]);
    CHECK(io_read(IO_FS_STATUS) == 0 && n == 31 && memcmp(&k4510_ram[0x100000], "hello from", 10) == 0, "load");
    mem_load(0x0300, (const uint8_t *)"nope.bin", 9); io_write(IO_FS_CMD, FS_STAT);
    CHECK(io_read(IO_FS_STATUS) == 1, "stat missing -> 1");
    mem_load(0x0300, (const uint8_t *)"out.bin", 8); mem_load(0x200000, (const uint8_t *)"K4510!", 6);
    W32(IO_FS_ADDR, 0x200000); W32(IO_FS_LEN, 6); io_write(IO_FS_CMD, FS_SAVE);
    CHECK(io_read(IO_FS_STATUS) == 0, "save");
    io_write(IO_FS_CMD, FS_STAT); CHECK(R32(IO_FS_SIZE) == 6, "saved size");
    io_write(IO_FS_CMD, FS_DIR_FIRST); int count = 0, seen = 0;
    for (;;) { W32(IO_FS_ADDR, 0x400); io_write(IO_FS_CMD, FS_DIR_NEXT); if (io_read(IO_FS_STATUS)) break; count++; if (!strcmp((char *)&k4510_ram[0x400], "out.bin")) seen = 1; }
    printf("2. DIR: %d entries, out.bin seen=%d\n", count, seen);
    CHECK(count >= 2 && seen, "dir");
    mem_load(0x0300, (const uint8_t *)"../etc/passwd", 14); io_write(IO_FS_CMD, FS_STAT);
    CHECK(io_read(IO_FS_STATUS) == 1, "sandbox");
    remove("fs/out.bin");
    fixture(0);
    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails); return fails != 0;
}
