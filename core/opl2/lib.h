/* K4510 shim for VICE's lib.h: its allocator, which is malloc with a check. */
#ifndef K4510_VICE_LIB_H
#define K4510_VICE_LIB_H
#include <stdlib.h>
static inline void *lib_malloc(size_t n) { return malloc(n); }
static inline void  lib_free(void *p) { free(p); }
#endif
