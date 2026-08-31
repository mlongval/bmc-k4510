/* K4510 shim: the clock the OPL2's two timers are set against.  Ours is the
 * machine's microsecond clock -- the same one FastSID ages its last store
 * against -- advanced by core/sid.cc as the sound is rendered. */
#ifndef K4510_VICE_MAINCPU_H
#define K4510_VICE_MAINCPU_H
#include "types.h"
extern CLOCK maincpu_clk;
#endif
