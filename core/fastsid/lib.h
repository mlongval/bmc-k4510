/* K4510 shim for VICE's lib.h: its allocator, which is malloc with a check. */
#ifndef K4510_VICE_LIB_H
#define K4510_VICE_LIB_H
#include <stdlib.h>
#include <string.h>
static inline void *lib_calloc(size_t n, size_t sz) { return calloc(n, sz); }
static inline void  lib_free(void *p) { free(p); }
static inline char *lib_stralloc(const char *s) { char *p = (char *)malloc(strlen(s) + 1); if (p) strcpy(p, s); return p; }
#endif
