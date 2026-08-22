/* What the K4510 core needs from whoever hosts it. Two implementations:
 * sdl/host_posix.c (Linux desktop) and pi/host_circle.cpp (bare-metal Pi).
 * Everything else the core uses is plain C library: fopen/opendir/stat for
 * the storage device, time() for the clock -- newlib on the Pi has them. */
#ifndef K4510_HOST_H
#define K4510_HOST_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *host_alloc_zeroed(size_t bytes);          /* NULL on failure; may be lazily committed */
void  host_zero(void *p, size_t bytes);         /* back to all-zero, releasing pages if it can */
void  host_poll_input(void);                    /* once per frame: the Pi scans the C64 keyboard here */
#ifdef __cplusplus
}
#endif
#endif
