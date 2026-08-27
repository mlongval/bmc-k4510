/* Boot-time clock calibration.  See calib.h for the idea; here is the work.
 *
 * The workload is a small 6502 loop in low RAM with the instruction mix of
 * an interpreter's inner loop -- absolute-indexed and zero-page loads, an
 * indirect-indexed load and store, an INC, a JSR/RTS, one I/O-page read, a
 * branch -- because that is what a BASIC, a Forth or an editor spends its
 * frame doing, and it is what the three-host sweep measured.  It is not a
 * worst case: a program doing heavy screen work has a different mix, and the
 * margin plus the frontend's step-down on audio gaps are what cover that.
 *
 * Sound is on -- a sawtooth gated on every SID -- for the reason the policy
 * note gives: reSID's cost is latched by the first write to a chip, not by
 * whether it sounds, and a silent measurement flatters the host by half a
 * millisecond, which is most of a clock step near the ceiling. */
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

/* The loop lives behind a bank register, not in low RAM.  Unmapped low RAM
 * is the core's one-compare fast path and a loop there measures about half
 * the true cost; a real program -- EhBASIC in its K4SG segments, VI with its
 * file in far memory, anything overlaid through the far gate -- fetches and
 * stores through the banked path, and that is the path the three-host sweep
 * priced.  Block 2 ($4000-$5FFF) onto physical $00100000: code at $4400,
 * its tables at $4600 and $4700, all banked; the zero page stays where it is. */
#define WORK_BLOCK 2
#define WORK_PHYS  0x00100000u
#define WORK_AT    0x4400u
static const uint8_t workload[] = {
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

/* the cost of a frame at hz: a few to warm the caches, then the mean of the
 * rest with the slowest one dropped (a scheduler hiccup is not the host) */
static double probe(unsigned hz, double (*now_ms)(void))
{
    enum { WARM = 2, N = 8 };
    double ms[N], sum = 0, worst = 0;
    sid_set_cpu_hz((double)hz);                     /* or reSID renders at the wrong rate by the clock multiple */
    for (int i = 0; i < WARM; i++) one_frame(hz, now_ms);
    for (int i = 0; i < N; i++) { ms[i] = one_frame(hz, now_ms); sum += ms[i]; if (ms[i] > worst) worst = ms[i]; }
    return (sum - worst) / (N - 1);
}

int calib_run(double (*now_ms)(void), const unsigned *steps_hz, int nsteps,
              double budget_ms, double margin, calib_result *r)
{
    memset(r, 0, sizeof *r);
    /* the workload, and the machine it runs on */
    mem_bank_set(WORK_BLOCK, WORK_PHYS);
    for (unsigned i = 0; i < sizeof workload; i++) mem_poke(WORK_PHYS + (WORK_AT - 0x4000u) + i, workload[i]);
    mem_poke(0x02, 3); mem_poke(0x03, 0);
    mem_poke(0x04, 0x00); mem_poke(0x05, 0x46);     /* ($04) -> $4600, banked */
    mem_poke(0x06, 0x00); mem_poke(0x07, 0x47);     /* ($06) -> $4700, banked */
    io_write(0xD000, 1);                            /* VICKY on, so its lines cost what they cost */
    sound_on();
    cpu65_reset();
    cpu65.pc = WORK_AT;

    /* two probes fix the line.  The first is the MEGA65's clock; where that
     * already fills most of a frame (a Pi), the second goes down rather than
     * up, so a slow host is never asked to run three times what it cannot. */
    unsigned a = 40500000u, b;
    double ma = probe(a, now_ms);
    b = ma > budget_ms * 0.6 ? 10000000u : 121500000u;
    double mb = probe(b, now_ms);
    r->probe_hz[0] = a; r->probe_ms[0] = ma; r->probe_hz[1] = b; r->probe_ms[1] = mb;

    double mhz_a = a / 1e6, mhz_b = b / 1e6;
    double per = (mb - ma) / (mhz_b - mhz_a);
    if (per < 1e-4) per = 1e-4;                     /* a host that fast is not measurable this way; be conservative */
    double fixed = ma - per * mhz_a; if (fixed < 0) fixed = 0;
    r->ms_per_mhz = per; r->ms_fixed = fixed;
    r->ceiling_mhz = (budget_ms - fixed) / per;

    double allow = budget_ms * margin;
    int chosen = -1;
    for (int i = 0; i < nsteps; i++) {              /* fastest first: the first that fits wins */
        double mhz = steps_hz[i] / 1e6;
        if (fixed + per * mhz <= allow) { chosen = i; break; }
    }
    mem_bank_off(WORK_BLOCK);                      /* the caller power-cycles the rest */
    int ok = chosen >= 0;
    if (!ok) chosen = nsteps - 1;
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
