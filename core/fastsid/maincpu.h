/* K4510 shim: VICE's CPU clock, which fastsid reads to age the last store
 * (the register-read delay).  The K4510 keeps its own; see fastsid_k4510.c. */
#ifndef K4510_VICE_MAINCPU_H
#define K4510_VICE_MAINCPU_H
#include "types.h"
extern CLOCK maincpu_clk;
#endif
