/* K4510 host glue for POSIX: 256 MB reserved with lazy commit. */
#include "../core/host.h"
#include <sys/mman.h>
void *host_alloc_zeroed(size_t bytes)
{
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
void host_zero(void *p, size_t bytes) { madvise(p, bytes, MADV_DONTNEED); }
void host_poll_input(void) { }
void host_perf_probe(char *out, unsigned n) { if (n) out[0] = 0; }
