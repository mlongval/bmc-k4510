/* Boot-time clock calibration.  See calib.h for the idea; here is the work.
 *
 * Two workloads, and the answer is the worse of them.  One measurement is not
 * enough, and the evidence is direct: the arithmetic sweep of 2026-08-27 put
 * ubuntu-s1's ceiling near 125 MHz, while BENCH -- whose sweep loop polls the
 * frame counter hard -- found Doc's laptop clean at 40.5 and starving at 81.
 * Same core, three-to-one in the ceiling, because the cost of an emulated MHz
 * depends entirely on what the guest spends it on.  So:
 *
 *   phase 0  an interpreter's inner loop: indexed and zero-page loads, an
 *            indirect-indexed load and store, INC, JSR/RTS, one I/O read.
 *            What a BASIC, a Forth or an editor does with a frame.
 *   phase 1  an I/O-bound loop: seven register reads and two writes an
 *            iteration.  What a program polling the raster or the keyboard
 *            does, and what BENCH itself does.
 *
 * Both run behind a bank register.  The first version of this file ran in
 * unmapped low RAM and measured 0.060 ms per emulated MHz where the sweep had
 * measured 0.124 for BASIC: unmapped RAM is the core's one-compare fast path,
 * and a real program -- EhBASIC in its K4SG segments, VI with its file in far
 * memory, anything through the far gate -- does not get it.
 *
 * Sound is on -- a sawtooth gated on every SID -- because reSID's cost is
 * latched by the first write to a chip, not by whether it sounds, and a silent
 * measurement flatters the host by half a millisecond, which is most of a
 * clock step near the ceiling.  The caller must io_reset() afterwards or the
 * machine pays that half-millisecond, in silence, for the whole session. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "calib.h"
#include "mem.h"
#include "io.h"
#include "vicky.h"
#include "sid.h"
#include "host.h"
#include "xemu/emutools_basicdefs.h"
#include "xemu/cpu65.h"

/* Behind block 2 ($4000-$5FFF) onto physical $00100000: code at $4400, its
 * tables at $4600 and $4700.  The zero page stays where it is. */
#define WORK_BLOCK 2
#define WORK_PHYS  0x00100000u
#define WORK_AT    0x4400u

static const uint8_t work_interp[] = {
    0xA2, 0x00,             /* 4400  LDX #0                                  */
    0xBD, 0x00, 0x46,       /* 4402  LDA $4600,X      loop:                  */
    0x65, 0x02,             /* 4405  ADC $02                                 */
    0x9D, 0x00, 0x47,       /* 4407  STA $4700,X                             */
    0xA4, 0x03,             /* 440A  LDY $03                                 */
    0xB1, 0x04,             /* 440C  LDA ($04),Y                             */
    0x49, 0x55,             /* 440E  EOR #$55                                */
    0x91, 0x06,             /* 4410  STA ($06),Y                             */
    0xE6, 0x03,             /* 4412  INC $03                                 */
    0xAD, 0x0D, 0xD5,       /* 4414  LDA $D50D        one I/O read, as programs do */
    0x20, 0x20, 0x44,       /* 4417  JSR $4420                               */
    0xE8,                   /* 441A  INX                                     */
    0xD0, 0xE5,             /* 441B  BNE loop                                */
    0x4C, 0x02, 0x44,       /* 441D  JMP loop                                */
    0x4A,                   /* 4420  LSR A            sub:                   */
    0x2A,                   /* 4421  ROL A                                   */
    0x60,                   /* 4422  RTS                                     */
};

/* Reads, apart from the raster-compare pair, which changes nothing the
 * machine does: IRQMASK is clear after reset and the caller holds irqLevel
 * at 0, so a compare sets a status bit and no more. */
static const uint8_t work_io[] = {
    0xA2, 0x00,             /* 4400  LDX #0                                  */
    0xAD, 0x0D, 0xD5,       /* 4402  LDA $D50D        loop: the frame counter*/
    0x8D, 0x02, 0xD0,       /* 4405  STA $D002        raster compare low     */
    0xAD, 0x02, 0xD0,       /* 4408  LDA $D002        the raster line        */
    0xAD, 0x20, 0xD6,       /* 440B  LDA $D620        which blocks are banked*/
    0xAD, 0x21, 0xD6,       /* 440E  LDA $D621        the MAP mask           */
    0xAD, 0x0E, 0xD5,       /* 4411  LDA $D50E                               */
    0x8D, 0x03, 0xD0,       /* 4414  STA $D003        compare high           */
    0xAD, 0x03, 0xD0,       /* 4417  LDA $D003                               */
    0xAD, 0x01, 0xD1,       /* 441A  LDA $D101        keyboard status        */
    0xE8,                   /* 441D  INX                                     */
    0xD0, 0xE2,             /* 441E  BNE loop                                */
    0x4C, 0x02, 0x44,       /* 4420  JMP loop                                */
};

static uint8_t fb[640 * 480];

static void sound_on(void)
{
    for (unsigned c = 0; c < 4; c++) {
        uint16_t b = (uint16_t)(0xD400 + c * 0x20);
        io_write(b + 0x18, 15);                     /* volume */
        io_write(b + 0x00, 0x00); io_write(b + 0x01, (uint8_t)(0x28 + c * 8));   /* a note, different per chip */
        io_write(b + 0x05, 0x00); io_write(b + 0x06, 0xF0);                     /* AD, SR */
        io_write(b + 0x04, 0x21);                   /* sawtooth, gate on */
    }
}

static void load_work(const uint8_t *code, unsigned n)
{
    for (unsigned i = 0; i < n; i++) mem_poke(WORK_PHYS + (WORK_AT - 0x4000u) + i, code[i]);
    mem_poke(0x02, 3); mem_poke(0x03, 0);
    mem_poke(0x04, 0x00); mem_poke(0x05, 0x46);     /* ($04) -> $4600, banked */
    mem_poke(0x06, 0x00); mem_poke(0x07, 0x47);     /* ($06) -> $4700, banked */
    cpu65_reset();
    cpu65.pc = WORK_AT;
}

static double one_frame(unsigned hz, double (*now_ms)(void))
{
    int cpl = (int)(hz / 60 / 480);
    double t0 = now_ms();
    vicky_begin_frame(fb, 640);
    for (int y = 0; y < 480; y++) {
        cpu65.irqLevel = 0;
        cpu65_step(cpl);
        vicky_line(y);
        { int16_t tmp[256]; sid_render(cpl, tmp, 256); }
    }
    vicky_end_frame();
    return now_ms() - t0;
}

/* the cost of a frame at hz: a couple to warm the caches, then the mean of the
 * rest with the slowest dropped (a scheduler hiccup is not the host) */
static double probe(unsigned hz, double (*now_ms)(void))
{
    enum { WARM = 2, N = 6 };
    double sum = 0, worst = 0;
    sid_set_cpu_hz((double)hz);                     /* or reSID renders at the wrong rate by the clock multiple */
    for (int i = 0; i < WARM; i++) one_frame(hz, now_ms);
    for (int i = 0; i < N; i++) { double m = one_frame(hz, now_ms); sum += m; if (m > worst) worst = m; }
    return (sum - worst) / (N - 1);
}

int calib_run(double (*now_ms)(void), const unsigned *steps_hz, int nsteps,
              double budget_ms, double margin, calib_result *r)
{
    memset(r, 0, sizeof *r);
    mem_bank_set(WORK_BLOCK, WORK_PHYS);
    io_write(0xD000, 1);                            /* VICKY on, so its lines cost what they cost */
    sound_on();

    for (int ph = 0; ph < 2; ph++) {
        load_work(ph ? work_io : work_interp, ph ? (unsigned)sizeof work_io : (unsigned)sizeof work_interp);
        /* two probes fix the line.  The first is the MEGA65's clock; where
         * that already fills much of a frame (a Pi), the second goes down
         * rather than up, so a slow host is never asked to run three times
         * what it plainly cannot. */
        unsigned a = 40500000u;
        double ma = probe(a, now_ms);
        unsigned b = ma > budget_ms * 0.6 ? 10000000u : 121500000u;
        double mb = probe(b, now_ms);
        double per = (mb - ma) / (b / 1e6 - a / 1e6);
        if (per < 1e-4) per = 1e-4;                 /* a host that fast is not measurable this way; be conservative */
        double fixed = ma - per * (a / 1e6); if (fixed < 0) fixed = 0;
        r->phase_per[ph] = per; r->phase_fixed[ph] = fixed;
        r->probe_hz[ph][0] = a; r->probe_ms[ph][0] = ma;
        r->probe_hz[ph][1] = b; r->probe_ms[ph][1] = mb;
    }
    mem_bank_off(WORK_BLOCK);                       /* the caller power-cycles the rest */

    /* the worse of the two, term by term: the clock has to survive both */
    r->ms_per_mhz = r->phase_per[0]   > r->phase_per[1]   ? r->phase_per[0]   : r->phase_per[1];
    r->ms_fixed   = r->phase_fixed[0] > r->phase_fixed[1] ? r->phase_fixed[0] : r->phase_fixed[1];
    r->ceiling_mhz = (budget_ms - r->ms_fixed) / r->ms_per_mhz;

    /* The ladder's order is the menu's business, not ours: take the highest
     * clock that fits, by value.  That enum has been reordered once already,
     * and a reader which assumed index 0 was the fastest was silent about
     * being wrong for a day (4f88833). */
    double allow = budget_ms * margin;
    int chosen = -1, slowest = 0;
    for (int i = 0; i < nsteps; i++) {
        double mhz = steps_hz[i] / 1e6;
        if (steps_hz[i] < steps_hz[slowest]) slowest = i;
        if (r->ms_fixed + r->ms_per_mhz * mhz <= allow &&
            (chosen < 0 || steps_hz[i] > steps_hz[chosen])) chosen = i;
    }
    int ok = chosen >= 0;
    if (!ok) chosen = slowest;
    r->step = chosen; r->step_hz = steps_hz[chosen];
    return ok ? 0 : -1;
}

/* FNV-1a over what the host says it is; the low 31 bits, never zero */
int calib_host_hash(void)
{
    char id[256]; host_fingerprint(id, sizeof id);
    uint32_t h = 2166136261u;
    for (const char *p = id; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    h &= 0x7FFFFFFFu;
    return h ? (int)h : 1;
}

/* A host that has not said what it is.  The Pi's Circle glue can override
 * this with the board revision; until it does, the cache is trusted on any
 * Pi, which is right for a card that stays in one machine. */
__attribute__((weak)) void host_fingerprint(char *out, unsigned n)
{
    snprintf(out, n, "unknown host");
}
