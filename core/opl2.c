/* The K4510's OPL2 -- a Yamaha YM3812 at $D480, nine FM voices.
 *
 * The design map has had "OPL2, DigiMAX" written against $D480 since the
 * machine was drawn; this is the OPL2 half arriving.  The chip is the AdLib's,
 * and it is wired the AdLib's way: an address port, a data port, and a status
 * register you poll.  Every AdLib instrument table and every OPL2 register
 * list ever written therefore means what it says on this machine.
 *
 *   $D480  W  ADDR    the register to write next
 *          R  STATUS  bit7 IRQ, bit6 timer 1 expired, bit5 timer 2 expired
 *   $D481  W  DATA    write it
 *          R          the last value written to the addressed register
 *   $D482  R  ID      $02 = an OPL2 is fitted, $00 = it is not
 *
 * The OPL2 and the SIDs are mutually exclusive -- Doc's rule, and the honest
 * one on a Pi, where the frame will not hold both.  core/ui picks; sid.cc is
 * muted while this chip has the sound.
 *
 * MAME's fmopl.c does the synthesis, by way of VICE (core/opl2/, unaltered).
 * What it wants around it is an allocator and two alarms for its timers, and
 * that is what the top of this file is.
 */
#include "opl2.h"
#include "vice_clk.h"
#include <string.h>
#include "opl2/fmopl.h"

/* ---- the two alarms fmopl.c sets its timers with ----------------------- *
 * VICE has a general alarm queue; the OPL2 uses exactly two of them, so this
 * is two slots and a poll rather than the machinery.  vice_clk_advance()
 * calls alarm_poll as the microsecond clock moves, which is what makes the
 * status register's overflow flags appear at the right time for a player
 * that waits on them. */
#define MAX_ALARMS 4
static alarm_t alarms[MAX_ALARMS];
static int nalarms;
alarm_context_t *maincpu_alarm_context;          /* VICE names one; we need none */

alarm_t *alarm_new(alarm_context_t *ctx, const char *name, alarm_callback_t cb, void *data)
{
    (void)ctx; (void)name;
    if (nalarms >= MAX_ALARMS) return 0;
    { alarm_t *a = &alarms[nalarms++]; a->cb = cb; a->data = data; a->set = 0; a->when = 0; return a; }
}
void alarm_destroy(alarm_t *a) { if (a) a->set = 0; }
void alarm_set(alarm_t *a, uint32_t when) { if (a) { a->when = when; a->set = 1; } }
void alarm_unset(alarm_t *a) { if (a) a->set = 0; }
void alarm_poll(uint32_t now)
{
    for (int i = 0; i < nalarms; i++) {
        alarm_t *a = &alarms[i];
        /* Unsigned wrap: due when it is not more than half the clock's range
         * in the future.  The clock is 32 bits of microseconds -- 71 minutes
         * -- and a machine left on for longer must not have its timers stop. */
        if (a->set && (uint32_t)(now - a->when) < 0x80000000u) {
            a->set = 0;
            a->cb((uint32_t)(now - a->when), a->data);   /* VICE passes the overshoot */
        }
    }
}

/* ---- the chip ---------------------------------------------------------- */
#define OPL2_HZ 3579545.0            /* the AdLib's crystal, which every register list assumes */
static FM_OPL *opl;
static int     opl_rate = 48000;
static int     opl_on;               /* the machine has the OPL2 selected */
static uint8_t opl_addr;             /* the address port's latch */
static uint8_t opl_shadow[256];      /* what was last written where, so DATA reads back */

void opl2_init(int rate)
{
    opl_rate = rate;
    if (opl) { ym3812_shutdown(opl); opl = 0; }
    nalarms = 0;
    fmopl_set_machine_parameter(K4510_VICE_CLK_HZ);   /* the timers count microseconds */
    opl = ym3812_init((uint32_t)OPL2_HZ, (uint32_t)rate);
    memset(opl_shadow, 0, sizeof opl_shadow);
    opl_addr = 0;
}
void opl2_reset(void)
{
    nalarms = 0;
    memset(opl_shadow, 0, sizeof opl_shadow);
    opl_addr = 0;
    if (opl) ym3812_reset_chip(opl);
}
void opl2_set_enabled(int on) { opl_on = on ? 1 : 0; }
int  opl2_enabled(void) { return opl_on; }

void opl2_write(uint8_t reg, uint8_t v)
{
    if (!opl) return;
    switch (reg) {
    case 0: opl_addr = v; ym3812_write(opl, 0, v); return;
    case 1: opl_shadow[opl_addr] = v; ym3812_write(opl, 1, v); return;
    default: return;
    }
}
uint8_t opl2_read(uint8_t reg)
{
    switch (reg) {
    case 0: return opl ? ym3812_read(opl, 0) : 0x00;      /* STATUS */
    case 1: return opl_shadow[opl_addr];
    case 2: return opl ? 0x02 : 0x00;                     /* an OPL2 is fitted */
    default: return 0xFF;
    }
}

/* n samples of FM, mixed down to the machine's one channel.  fmopl renders
 * into its own buffer and we scale: the OPL2 swings a good deal wider than a
 * SID and would clip the mix at full tilt. */
#define OPL2_BLOCK 1024
int opl2_render(int n, int16_t *out, int max)
{
    static OPLSAMPLE tmp[OPL2_BLOCK];
    int done = 0;
    if (n > max) n = max;
    if (n <= 0) return 0;
    if (!opl || !opl_on) { for (int i = 0; i < n; i++) out[i] = 0; return n; }
    while (done < n) {
        int want = n - done;
        if (want > OPL2_BLOCK) want = OPL2_BLOCK;
        ym3812_update_one(opl, tmp, want);
        for (int i = 0; i < want; i++) {
            int v = tmp[i] / 2;                            /* headroom, as the SID mix has */
            out[done + i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        done += want;
    }
    return done;
}
