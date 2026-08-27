/* The CPU clock is measured, not guessed.
 *
 * docs/CPU-CLOCK-POLICY.md, proposal 1: at power-on, before the shell, run
 * the real core over a fixed workload with sound on, time it, and choose the
 * highest step of the clock ladder that fits a frame with margin.  The
 * emulated frame's cost is a straight line in the clock -- a fixed part
 * (VICKY, reSID) plus a cost per emulated MHz (the core) -- so two probes
 * fix the line and the ceiling falls out.  The three-host sweep of
 * 2026-08-27 (BUILD-LOG) is where the shape was established.
 *
 * The caller owns the machine: it is left dirty (a workload in low RAM, the
 * SIDs written, VICKY switched on) and must be power-cycled afterwards --
 * io_reset() in particular, because a SID once written is rendered until
 * sid_reset() whether or not it sounds. */
#ifndef K4510_CALIB_H
#define K4510_CALIB_H
#ifdef __cplusplus
extern "C" {
#endif
/* Two phases -- an interpreter's mix and an I/O-bound one -- because the cost
 * of an emulated MHz depends on what the guest spends it on, by as much as
 * three to one.  The chosen clock has to survive both, so ms_fixed and
 * ms_per_mhz are the worse of the two, term by term. */
enum { CALIB_INTERP, CALIB_IO, CALIB_PHASES };
typedef struct {
    double   ms_fixed;        /* the frame's cost at 0 MHz: VICKY, reSID, the rest */
    double   ms_per_mhz;      /* what one emulated MHz costs this host, per frame */
    double   ceiling_mhz;     /* where the line crosses the budget */
    int      step;            /* the chosen index into steps_hz */
    unsigned step_hz;
    double   phase_per[CALIB_PHASES], phase_fixed[CALIB_PHASES];
    unsigned probe_hz[CALIB_PHASES][2];   /* the clocks tried, and what a frame cost at each */
    double   probe_ms[CALIB_PHASES][2];
} calib_result;

/* now_ms: the host's monotonic clock, in milliseconds (fractional).
 * steps_hz: the ladder, in any order -- the highest that fits wins, by value.
 * budget_ms: the frame (16.67).
 * margin: how much of the budget the chosen step may use (0.7 leaves room for
 * a program heavier than the workload, and for the frontend's own share of
 * the frame).  Returns 0 and fills r; -1 if the host is too slow for even the
 * lowest step, with r->step the slowest of the ladder anyway. */
int  calib_run(double (*now_ms)(void), const unsigned *steps_hz, int nsteps,
               double budget_ms, double margin, calib_result *r);

/* A number that changes when the host does -- CPU model and count on the
 * desktop, the board on the Pi -- so a cached measurement is trusted only on
 * the machine it was taken on.  Never 0, which means "never measured". */
int  calib_host_hash(void);
#ifdef __cplusplus
}
#endif
#endif
