/* FastSID on the K4510 -- the machine's four SIDs, done VICE's cheap way.
 *
 * reSID (core/resid, Dag Lem) models the chip cycle by cycle and sounds like
 * one; FastSID (VICE, Teemu Rantanen and others) steps its oscillators once
 * per OUTPUT SAMPLE from wavetables, which is roughly a twentieth of the work
 * at 48 kHz and does not sound the same.  The K4510 carries both because the
 * Pi cannot always afford the first: four reSIDs are ~3.4 ms of a 16.7 ms
 * frame on a 3B+, and four FastSIDs are nearer a fifth of that.
 *
 * The two engines are mutually exclusive and share everything the guest can
 * see: the same four chips at $D400, the same registers, the same shadow.
 * core/sid.cc is the dispatcher; this file is only the FastSID half.
 *
 * VICE's fastsid.c is used unaltered.  What it wants from the emulator around
 * it is small -- an allocator, a CPU clock, and two settings -- and that is
 * supplied here and by the shim headers in core/fastsid/.
 */
#include "sid.h"
#include <string.h>
#include <stdint.h>
#include "fastsid/fastsid.h"

/* ---- what VICE's fastsid.c reaches for -------------------------------- */
/* Its CPU clock.  fastsid_read() ages the last store against it to give the
 * register-read behaviour a program can measure, so it has to advance with
 * the chips; fsid_render does that. */
CLOCK maincpu_clk = 0;

static int fsid_filters = 1;             /* the filter emulation: always on */
static int fsid_model_now = 0;           /* 0 = 6581, 1 = 8580; see fsid_set_model */

int resources_get_int(const char *name, int *value)
{
    if (!strcmp(name, "SidFilters")) { *value = fsid_filters; return 0; }
    if (!strcmp(name, "SidModel") || !strcmp(name, "Sid2Model")) { *value = fsid_model_now; return 0; }
    return -1;
}
/* VICE's mixer tells an engine where in the buffer it is; ours renders whole
 * blocks and never asks a chip to start part-way in. */
int sound_sample_position(void) { return 0; }

/* ---- the four chips ---------------------------------------------------- */
static sound_t *chip[K4510_SIDS];
static uint8_t  shadow[K4510_SIDS][32];      /* fastsid_open reads the registers from here */
static uint8_t  model[K4510_SIDS];           /* per chip: 0 = 6581, 1 = 8580 */
static double   fsid_hz = 1000000.0;         /* the chips' clock */
static int      fsid_rate = 48000;
static int      opened;
static int32_t  dcb_x1, dcb_y1;              /* the DC blocker's state; see dc_block */

static void open_chip(int c)
{
    if (chip[c]) { fastsid_hooks.close(chip[c]); chip[c] = 0; }
    fsid_model_now = model[c];                       /* fastsid_init asks for "SidModel" */
    chip[c] = fastsid_hooks.open(shadow[c]);
    if (chip[c]) fastsid_hooks.init(chip[c], fsid_rate, (int)fsid_hz, 1000);
}

void fsid_init(double sid_hz, int rate)
{
    fsid_hz = sid_hz; fsid_rate = rate;
    for (int c = 0; c < K4510_SIDS; c++) open_chip(c);
    opened = 1;
}
/* The chip clock changed (PAL/NTSC), or the sample rate did: FastSID folds
 * both into speed1 at init time, so the chips are rebuilt.  Their register
 * shadow survives, which is what a tune actually depends on. */
void fsid_set_clock(double sid_hz)
{
    if (sid_hz == fsid_hz || !opened) { fsid_hz = sid_hz; return; }
    fsid_hz = sid_hz;
    for (int c = 0; c < K4510_SIDS; c++) open_chip(c);
}
void fsid_reset(void)
{
    maincpu_clk = 0; dcb_x1 = dcb_y1 = 0;
    memset(shadow, 0, sizeof shadow);
    for (int c = 0; c < K4510_SIDS; c++) if (chip[c]) fastsid_hooks.reset(chip[c], 0);
}
void fsid_set_model(int c, int mos8580)
{
    if (c < 0 || c >= K4510_SIDS || model[c] == (uint8_t)(mos8580 ? 1 : 0)) return;
    model[c] = (uint8_t)(mos8580 ? 1 : 0);
    if (opened) open_chip(c);                        /* the wavetable choice is made in init */
}
void fsid_write(int c, uint8_t r, uint8_t v)
{
    if (c < 0 || c >= K4510_SIDS) return;
    shadow[c][r & 0x1F] = v;
    if (chip[c]) fastsid_hooks.store(chip[c], (uint16_t)(r & 0x1F), v);
}
uint8_t fsid_read(int c, uint8_t r)
{
    if (c < 0 || c >= K4510_SIDS || !chip[c]) return 0xFF;
    return fastsid_hooks.read(chip[c], (uint16_t)(r & 0x1F));
}

/* FastSID's mix is not centred on zero: a silent voice still contributes its
 * own constant, so a one-voice note sits well off centre -- measured at about
 * -5,400 of 32,767 for the sawtooth in test/sidtest.  reSID's is centred, so
 * without this a switch between engines would be a step in the waveform, which
 * is a click, and the offset would eat a sixth of the headroom besides.  One
 * pole at about 4 Hz takes it out and nothing audible with it: the C64's own
 * output capacitor did the same job.
 * Fixed point, 16 fractional bits; R = 0.9995 at 48 kHz. */
static int dc_block(int x)
{
    int32_t y = (int32_t)x - dcb_x1 + (int32_t)(((int64_t)dcb_y1 * 65503) >> 16);
    dcb_x1 = x; dcb_y1 = y;
    return (int)y;
}

/* n samples, mixed.  Unlike reSID every chip is asked for the same count and
 * hands back exactly that -- FastSID advances per sample, not per cycle -- so
 * there is no phase between chips to reconcile here (see core/sid.cc, where
 * reSID's phases have to be). */
#define FSID_BLOCK 1024
int fsid_render(int n, int16_t *out, int max, int nmax, const int *sounding)
{
    static int16_t tmp[K4510_SIDS][FSID_BLOCK];
    int done = 0;
    if (n > max) n = max;
    if (n <= 0) return 0;
    /* In blocks, because the per-chip scratch is fixed and asking for more
     * than it holds must not quietly lose the rest -- reSID's mix lost
     * samples that way and it cost an afternoon (2026-08-30). */
    while (done < n) {
        int want = n - done, c, i, got[K4510_SIDS], any = 0;
        if (want > FSID_BLOCK) want = FSID_BLOCK;
        for (c = 0; c < K4510_SIDS; c++) {
            got[c] = 0;
            if (c >= nmax || !sounding[c] || !chip[c]) continue;
            { int dt = 0; got[c] = fastsid_hooks.calculate_samples(chip[c], tmp[c], want, 1, &dt); }
            any = 1;
        }
        for (i = 0; i < want; i++) {
            int v = 0;
            if (any) for (c = 0; c < K4510_SIDS; c++) if (got[c] > i) v += tmp[c][i];
            v /= 2;                                  /* four chips: headroom, as reSID's mix has */
            v = dc_block(v);
            out[done + i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        done += want;
    }
    maincpu_clk += (CLOCK)((double)done * fsid_hz / fsid_rate);
    return done;
}
