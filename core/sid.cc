#include "resid/sid.h"
extern "C" {
#include "sid.h"
}
#include <string.h>

static reSID::SID chips[K4510_SIDS];
static double cpu_hz = 40500000.0; static int rate = 48000;

extern "C" void sid_init(double hz, int sample_rate)
{
    cpu_hz = hz; rate = sample_rate;
    for (int i = 0; i < K4510_SIDS; i++) {
        chips[i].set_chip_model(reSID::MOS6581);
        chips[i].set_sampling_parameters(cpu_hz, reSID::SAMPLE_FAST, rate);   /* FAST: cheap, fine for 40 MHz */
        chips[i].reset();
    }
}
extern "C" void sid_reset(void) { for (int i = 0; i < K4510_SIDS; i++) chips[i].reset(); }
extern "C" void sid_write(int c, uint8_t r, uint8_t v) { if (c >= 0 && c < K4510_SIDS) chips[c].write(r & 0x1F, v); }
extern "C" uint8_t sid_read(int c, uint8_t r) { return (c >= 0 && c < K4510_SIDS) ? chips[c].read(r & 0x1F) : 0xFF; }
extern "C" void sid_set_model(int c, int m8580) { if (c >= 0 && c < K4510_SIDS) chips[c].set_chip_model(m8580 ? reSID::MOS8580 : reSID::MOS6581); }

extern "C" int sid_render(int cycles, int16_t *out, int max)
{
    static short tmp[K4510_SIDS][4096];
    int n = 0;
    for (int c = 0; c < K4510_SIDS; c++) {
        reSID::cycle_count dt = cycles;
        int got = chips[c].clock(dt, tmp[c], max < 4096 ? max : 4096);
        if (got > n) n = got;
    }
    for (int i = 0; i < n; i++) {
        int v = 0;
        for (int c = 0; c < K4510_SIDS; c++) v += tmp[c][i];
        v /= 2;                                   /* 4 chips: headroom */
        out[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
    }
    return n;
}
