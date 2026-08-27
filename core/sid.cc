#include "resid/sid.h"
extern "C" {
#include "sid.h"
}
#include <string.h>

static reSID::SID chips[K4510_SIDS];
static double cpu_hz = 40500000.0; static int rate = 48000;
static bool active[K4510_SIDS];                  /* written since reset: clocked and mixed */
/* The SIDs run at SID_HZ like the real chip, whatever the CPU clock: the
 * frequency/ADSR registers keep their C64 meaning (f = reg * SID_HZ / 2^24)
 * and reSID does 1/40th of the work. sid_render receives CPU cycles. */
static double SID_HZ = 1000000.0;
static int sid_rate_saved = 48000;
static double sid_acc = 0;

extern "C" void sid_set_clock(int sel)          /* 0 = 1 MHz, 1 = PAL C64 (985248), 2 = NTSC (1022730) */
{
    SID_HZ = sel == 1 ? 985248.0 : sel == 2 ? 1022730.0 : 1000000.0;
    for (int i = 0; i < 4; i++) chips[i].set_sampling_parameters(SID_HZ, reSID::SAMPLE_FAST, sid_rate_saved);
}

extern "C" void sid_init(double hz, int sample_rate)
{
    sid_rate_saved = sample_rate;
    cpu_hz = hz; rate = sample_rate;
    for (int i = 0; i < K4510_SIDS; i++) {
        chips[i].set_chip_model(reSID::MOS6581);
        chips[i].set_sampling_parameters(SID_HZ, reSID::SAMPLE_FAST, rate);
        chips[i].reset();
    }
}
extern "C" void sid_set_cpu_hz(double hz) { cpu_hz = hz; }   /* the CPU clock changed: same SID clock, different ratio */
extern "C" void sid_reset(void) { sid_acc = 0; for (int i = 0; i < K4510_SIDS; i++) { chips[i].reset(); active[i] = false; } }
extern "C" void sid_write(int c, uint8_t r, uint8_t v) { if (c >= 0 && c < K4510_SIDS) { chips[c].write(r & 0x1F, v); active[c] = true; } }
extern "C" uint8_t sid_read(int c, uint8_t r) { return (c >= 0 && c < K4510_SIDS) ? chips[c].read(r & 0x1F) : 0xFF; }
extern "C" void sid_set_model(int c, int m8580) { if (c >= 0 && c < K4510_SIDS) chips[c].set_chip_model(m8580 ? reSID::MOS8580 : reSID::MOS6581); }

extern "C" int sid_render(int cycles, int16_t *out, int max)
{
    static short tmp[K4510_SIDS][4096];
    int n = 0;
    sid_acc += (double)cycles * SID_HZ / cpu_hz;
    int sid_cycles = (int)sid_acc; sid_acc -= sid_cycles;
    if (sid_cycles <= 0) return 0;
    /* Only chips the machine has written since reset are clocked. reSID costs
     * the same for silence as for sound, and at the prompt -- or in SIDPLAY,
     * which uses one chip -- three of the four are silent: on a Pi 3B+ that
     * was 2.5 ms of every 16.7 ms frame. The mix and its headroom are
     * unchanged, so a tune sounds exactly as it did. */
    for (int c = 0; c < K4510_SIDS; c++) {
        if (!active[c]) continue;
        reSID::cycle_count dt = sid_cycles;
        int got = chips[c].clock(dt, tmp[c], max < 4096 ? max : 4096);
        if (got > n) n = got;
    }
    if (!n) {                                     /* nothing active: silence at the rate the chips would have given */
        n = (int)((double)sid_cycles * rate / SID_HZ); if (n > max) n = max;
        for (int i = 0; i < n; i++) out[i] = 0;
        return n;
    }
    for (int i = 0; i < n; i++) {
        int v = 0;
        for (int c = 0; c < K4510_SIDS; c++) if (active[c]) v += tmp[c][i];
        v /= 2;                                   /* 4 chips: headroom */
        out[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
    }
    return n;
}
