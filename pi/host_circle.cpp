// K4510 host glue for Circle: the 256 MB comes from the kernel heap (1 GB
// on a 3B+), the per-frame input hook scans the C64 keyboard.
#include "../core/host.h"
#include <cstdlib>
#include <cstring>
extern "C" void c64kbd_poll(void);
extern "C" void *host_alloc_zeroed(size_t bytes) { void *p = malloc(bytes); if (p) memset(p, 0, bytes); return p; }
extern "C" void  host_zero(void *p, size_t bytes) { memset(p, 0, bytes); }
extern "C" void  host_poll_input(void) { c64kbd_poll(); }
