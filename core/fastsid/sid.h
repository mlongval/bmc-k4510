/* K4510 shim: VICE dispatches its SID engines through this table of hooks.
 * The K4510 calls the engine's functions directly, but fastsid.c ends with
 * the table, so the type has to exist. */
#ifndef K4510_VICE_SID_H
#define K4510_VICE_SID_H
#include <stdint.h>
#include "types.h"
#include "sound.h"
#include "sid-snapshot.h"
typedef struct sid_engine_s {
    sound_t *(*open)(uint8_t *sidstate);
    int      (*init)(sound_t *psid, int speed, int cycles_per_sec, int factor);
    void     (*close)(sound_t *psid);
    uint8_t  (*read)(sound_t *psid, uint16_t addr);
    void     (*store)(sound_t *psid, uint16_t addr, uint8_t byte);
    void     (*reset)(sound_t *psid, CLOCK cpu_clk);
    int      (*calculate_samples)(sound_t *psid, short *pbuf, int nr, int interleave, int *delta_t);
    void     (*prevent_clk_overflow)(sound_t *psid, CLOCK sub);
    char    *(*dump_state)(sound_t *psid);
    void     (*state_read)(sound_t *psid, struct sid_snapshot_state_s *sid_state);
    void     (*state_write)(sound_t *psid, struct sid_snapshot_state_s *sid_state);
} sid_engine_t;
#endif
