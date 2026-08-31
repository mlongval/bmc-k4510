/* K4510 sound: four reSID chips at IO_SID ($D400), $20 bytes each; the
 * Audio menu clocks 1-4 of them (sid_set_max).
 * C wrapper over the vendored C++ reSID (core/resid/, Dag Lem, GPL2+).
 */
#ifndef K4510_SID_H
#define K4510_SID_H
#include <stdint.h>
#define K4510_SIDS 4
/* The two engines are mutually exclusive: reSID (core/resid) models the chip
 * cycle by cycle and sounds like one; FastSID (core/fastsid, from VICE) steps
 * once per output sample from wavetables for about a twentieth of the cost.
 * Both drive the same four chips at $D400 and the same registers, so a switch
 * is heard, not seen -- sid_set_engine replays the registers into the engine
 * it turns on, and a tune plays through it. */
#define SID_ENGINE_RESID 0
#define SID_ENGINE_FAST  1
#ifdef __cplusplus
extern "C" {
#endif
void sid_set_max(int n);      /* clock/mix only the first n chips: the Machine menu's Active SIDs */
void sid_set_engine(int e);   /* SID_ENGINE_RESID or SID_ENGINE_FAST */
int  sid_get_engine(void);
void sid_set_mute(int mute);  /* the OPL2 has the sound: clock nothing, mix silence */
void sid_drain_to(uint32_t us);  /* the rendering core: perform every queued write due by then (core/sidq.h) */
#ifdef __cplusplus
}
#endif
void    sid_init(double cpu_hz, int sample_rate);
void    sid_set_clock(int sel);     /* 0 = 1 MHz (default), 1 = PAL C64 (985248), 2 = NTSC (1022730) */
void    sid_reset(void);
void    sid_set_cpu_hz(double hz);   /* the CPU clock changed; the SID clock did not */
void    sid_write(int chip, uint8_t reg, uint8_t v);
uint8_t sid_read(int chip, uint8_t reg);
void    sid_set_model(int chip, int mos8580);
/* Advance all chips by cpu cycles and mix their output into out[] (mono, 16-bit);
 * returns samples produced (<= max). Call from the audio side with the cycles elapsed. */
int     sid_render(int cycles, int16_t *out, int max);
#endif
