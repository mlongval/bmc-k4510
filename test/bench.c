/* Headless frame-time benchmark: how much host time does one emulated
 * frame cost, split into CPU / VICKY / reSID?  bench ROM FRAMES "keys"
 * Keys are typed one per frame from frame 5 (like capture); timing starts
 * after WARMUP frames so the program being measured is already running. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicky.h"
#include "../core/sid.h"
#define WARMUP 120
static uint8_t fb[640 * 480];
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv)
{
    const char *rom = argc > 1 ? argv[1] : "rom/kernal.bin"; int frames = argc > 2 ? atoi(argv[2]) : 600; const char *keys = argc > 3 ? argv[3] : "";
    int ki = 0, kn = strlen(keys);
    uint8_t font[2048]; FILE *ff = fopen("data/font8.bin", "rb"); if (!ff || fread(font, 1, 2048, ff) != 2048) return 1; fclose(ff);
    if (mem_init()) return 1; fs_set_root("fs"); mem_load(K4510_FONT8_PHYS, font, 2048); if (mem_load_rom(rom) <= 0) return 1;
    io_reset(); cpu65_reset();
    double tc = 0, tv = 0, ts = 0, tp = 0; int measured = 0; uint32_t rgb[640];
    for (int fr = 0; fr < WARMUP + frames; fr++) {
        if (fr >= 5 && ki < kn) { uint8_t k = (uint8_t)keys[ki++]; kbd_push(k == '\n' ? 0x0D : k); }
        int m = fr >= WARMUP; double t0, t1;
        vicky_begin_frame(fb, 640);
        for (int y = 0; y < 480; y++) {
            cpu65.irqLevel = vicky_irq() ? 1 : 0;
            t0 = now(); cpu65_step(40500000 / 60 / 480); t1 = now(); if (m) tc += t1 - t0;
            t0 = t1; vicky_line(y); t1 = now(); if (m) tv += t1 - t0;
            { int16_t tmp[256]; t0 = t1; sid_render(40500000 / 60 / 480, tmp, 256); t1 = now(); if (m) ts += t1 - t0; }
        }
        vicky_end_frame();
        /* palette expansion, as the SDL frontend does it (the Pi would write 8-bit directly) */
        t0 = now(); for (int y = 0; y < 480; y++) for (int x = 0; x < 640; x++) rgb[x] = vicky_palette_rgb(fb[y * 640 + x]); t1 = now(); if (m) tp += t1 - t0;
        if (m) measured++;
    }
    (void)rgb;
    double f = 1000.0 / measured;
    printf("%-16s %4d frames: cpu %6.2f ms  vicky %6.2f ms  resid %6.2f ms  palette %5.2f ms  = %6.2f ms/frame (budget 16.67)\n",
           kn ? keys : "(idle shell)", measured, tc * f, tv * f, ts * f, tp * f, (tc + tv + ts + tp) * f);
    return 0;
}
