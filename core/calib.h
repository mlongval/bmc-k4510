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
typedef struct {
    double   ms_fixed;        /* the frame's cost at 0 MHz: VICKY, reSID, the rest */
    double   ms_per_mhz;      /* what one emulated MHz costs this host, per frame */
    double   ceiling_mhz;     /* where the line crosses the budget */
    int      step;            /* the chosen index into steps_hz */
    unsigned step_hz;
    unsigned probe_hz[2];     /* the two clocks tried, and what a frame cost at each */
    double   probe_ms[2];
} calib_result;

/* now_ms: the host's monotonic clock, in milliseconds (fractional).
 * steps_hz: the ladder, fastest first.  budget_ms: the frame (16.67).
 * margin: how much of the budget the chosen step may use (0.7 leaves room for
 * a program heavier than the workload, and for the frontend's own share of
 * the frame).  Returns 0 and fills r; -1 if the host is too slow for even
 * the lowest step, with r->step = nsteps - 1 anyway. */
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
