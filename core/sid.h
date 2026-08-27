/* K4510 sound: four reSID chips at IO_SID ($D400), $20 bytes each.
 * C wrapper over the vendored C++ reSID (core/resid/, Dag Lem, GPL2+).
 */
#ifndef K4510_SID_H
#define K4510_SID_H
#include <stdint.h>
#define K4510_SIDS 4
#ifdef __cplusplus
extern "C" {
#endif
void sid_set_max(int n);   /* clock/mix only the first n chips: the Machine menu's Active SIDs */
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
