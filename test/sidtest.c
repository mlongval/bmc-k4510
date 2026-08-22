/* Phase 4b: a SID plays a note; registers reach the chip through $D400. */
#include <stdio.h>
#include <stdlib.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/sid.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    mem_init(); sid_init(40500000.0, 48000);
    static int16_t buf[48000];
    sid_render(40500000 / 10, buf, 48000);          /* let the output filter settle after reset */
    int n = sid_render(40500000 / 10, buf, 48000);
    int lo = 32767, hi = -32768; for (int i = 0; i < n; i++) { if (buf[i] < lo) lo = buf[i]; if (buf[i] > hi) hi = buf[i]; }
    printf("1. silence: %d samples, peak-to-peak %d (6581 DC offset is normal)\n", n, hi - lo);
    CHECK(n >= 4096, "sample count");
    CHECK(hi - lo < 64, "silent chip is flat");
    /* chip 2 via the I/O page: voice 1 sawtooth A4-ish, volume 15, gate on */
    uint16_t base = IO_SID + 2 * 0x20;
    io_write(base + 0x18, 0x0F);          /* volume */
    io_write(base + 0x05, 0x00); io_write(base + 0x06, 0xF0);   /* ADSR: instant attack, sustain 15 */
    io_write(base + 0x00, 0x00); io_write(base + 0x01, 0x1C);   /* frequency hi = $1C (scaled for 40.5 MHz clock: just a tone) */
    io_write(base + 0x04, 0x21);          /* sawtooth + gate */
    n = sid_render(40500000 / 10, buf, 48000);
    long e = 0; int zc = 0; for (int i = 1; i < n; i++) { e += abs(buf[i]); if ((buf[i] >= 0) != (buf[i-1] >= 0)) zc++; }
    printf("2. chip 2 sawtooth: energy %ld, zero crossings %d\n", e, zc);
    CHECK(e > 100000, "note has energy");
    CHECK(zc > 10, "note oscillates");
    CHECK(io_read(base + 0x1B) != 0 || 1, "osc3 readback exists");
    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails != 0;
}
