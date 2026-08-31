/* K4510 shim: VICE's save states.  fmopl.c's snapshot functions are inside
 * `#if 0` upstream, so only the type is needed. */
#ifndef K4510_VICE_SNAPSHOT_H
#define K4510_VICE_SNAPSHOT_H
struct snapshot_s;
typedef struct snapshot_s snapshot_t;
#endif
