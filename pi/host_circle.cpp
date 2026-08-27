// K4510 host glue for Circle: the 256 MB comes from the kernel heap (1 GB
// on a 3B+), the per-frame input hook scans the C64 keyboard.
#include "../core/host.h"
#include <cstdlib>
#include <cstring>
extern "C" void c64kbd_poll(void);
extern "C" void *host_alloc_zeroed(size_t bytes) { void *p = malloc(bytes); if (p) memset(p, 0, bytes); return p; }
extern "C" void  host_zero(void *p, size_t bytes) { memset(p, 0, bytes); }
extern "C" void  host_poll_input(void) { c64kbd_poll(); }

// ---- PERF.TXT's Pi section ------------------------------------------------
// The firmware's own numbers, asked through the mailbox on core 0: the ARM
// clock it is actually running at, the most it will give, the SoC temperature,
// and the throttle flags (bit 0 under-voltage now, bit 1 frequency capped now,
// bit 2 throttled now, bit 3 soft temperature limit; bits 16-19 the same, ever
// since boot).  Then three probes run here on the emulator's own core: a
// register-only spin, timed against the generic timer, for the clock as this
// core experiences it; and a sequential read of the machine's RAM and of a
// static buffer, for whether the caches are on -- a Pi 3 reads cached memory
// at gigabytes a second and uncached at tens of megabytes.
#include <circle/bcmpropertytags.h>
#include <cstdint>
#include <SDL2/SDL_circle.h>      /* SDL2Circle_CallOn0 */
#include <cstdio>
#include <cstring>
extern "C" uint8_t *k4510_ram;
static uint32_t s_arm_hz, s_arm_max_hz, s_temp_mc, s_throttled; static bool s_mbox_ok;
static void mbox_probe(void *)
{
    CBcmPropertyTags tags;
    TPropertyTagClockRate cr; cr.nClockId = CLOCK_ID_ARM;
    if (tags.GetTag(PROPTAG_GET_CLOCK_RATE, &cr, sizeof cr, 4)) s_arm_hz = cr.nRate;
    TPropertyTagClockRate mx; mx.nClockId = CLOCK_ID_ARM;
    if (tags.GetTag(PROPTAG_GET_MAX_CLOCK_RATE, &mx, sizeof mx, 4)) s_arm_max_hz = mx.nRate;
    TPropertyTagTemperature t; t.nTemperatureId = 0;
    if (tags.GetTag(PROPTAG_GET_TEMPERATURE, &t, sizeof t, 4)) s_temp_mc = t.nValue;
    TPropertyTagSimple th; th.nValue = 0;
    if (tags.GetTag(PROPTAG_GET_THROTTLED, &th, sizeof th, 4)) s_throttled = th.nValue;
    s_mbox_ok = true;
}
static inline uint64_t cnt(void) { uint64_t v; asm volatile("mrs %0, cntvct_el0" : "=r"(v)); return v; }
static inline uint64_t cntf(void) { uint64_t v; asm volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v; }
static uint8_t s_static_buf[1 << 20];
extern "C" void host_perf_probe(char *out, unsigned n)
{
    s_mbox_ok = false;
    SDL2Circle_CallOn0(mbox_probe, nullptr);
    // 1. a dependent subtract-and-branch loop: about two cycles an iteration on
    //    a Cortex-A53, so the clock is roughly 2 * N / t
    const uint64_t N = 20000000;
    uint64_t t0 = cnt();
    { uint64_t i = N; asm volatile("1: subs %0, %0, #1\n bne 1b" : "+r"(i) :: "cc"); }
    uint64_t t1 = cnt();
    double spin_mhz = (double)(2 * N) * (double)cntf() / (double)(t1 - t0) / 1e6;
    // 2. read 4 MB of the machine's RAM, one byte a cache line
    uint64_t sum = 0; const unsigned MB4 = 4u << 20;
    t0 = cnt(); for (unsigned i = 0; i < MB4; i += 64) sum += k4510_ram[i]; t1 = cnt();
    double ram_mbs = (double)MB4 / ((double)(t1 - t0) / (double)cntf()) / 1e6;
    // 3. the same over a static buffer of this image
    t0 = cnt(); for (unsigned i = 0; i < sizeof s_static_buf; i += 64) sum += s_static_buf[i]; t1 = cnt();
    double bss_mbs = (double)sizeof s_static_buf / ((double)(t1 - t0) / (double)cntf()) / 1e6;
    snprintf(out, n,
        "\nThe Pi, from the firmware:\n"
        "  ARM clock now  %5u MHz   (maximum %u MHz)\n"
        "  SoC temperature %4.1f C\n"
        "  throttle flags  $%05X   (bit0 under-voltage, bit1 freq capped, bit2 throttled, bit3 soft temp limit; <<16 = ever since boot)\n"
        "The Pi, measured on the emulator's core:\n"
        "  spin-loop clock %5.0f MHz   (register-only loop; ~2 cycles an iteration)\n"
        "  machine RAM read %6.0f MB/s   (cached: thousands; uncached: tens)\n"
        "  static data read %6.0f MB/s\n%s",
        s_arm_hz / 1000000u, s_arm_max_hz / 1000000u, s_temp_mc / 1000.0, s_throttled,
        spin_mhz, ram_mbs, bss_mbs, s_mbox_ok ? "" : "  (the mailbox did not answer)\n");
    (void)sum;
}
