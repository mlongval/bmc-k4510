/* FastSID: the second SID engine (core/fsid.c wraps VICE's fastsid.c).
 * core/sid.cc is the only caller -- everything else talks to sid.h. */
#ifndef K4510_FSID_H
#define K4510_FSID_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void    fsid_init(double sid_hz, int rate);
void    fsid_set_clock(double sid_hz);
void    fsid_reset(void);
void    fsid_set_model(int chip, int mos8580);
void    fsid_write(int chip, uint8_t reg, uint8_t v);
uint8_t fsid_read(int chip, uint8_t reg);
int     fsid_render(int n, int16_t *out, int max, int nmax, const int *sounding);
#ifdef __cplusplus
}
#endif
#endif
