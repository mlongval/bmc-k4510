/* K4510 shim for VICE's sound.h: the opaque per-chip type (fastsid.c defines
 * struct sound_s itself) and the one call it makes back into the mixer. */
#ifndef K4510_VICE_SOUND_H
#define K4510_VICE_SOUND_H
struct sound_s;
typedef struct sound_s sound_t;
int sound_sample_position(void);
#endif
