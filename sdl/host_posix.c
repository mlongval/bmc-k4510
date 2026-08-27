#include <stdio.h>
#include <string.h>
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
/* the first "model name" in /proc/cpuinfo, and how many processors it lists */
void host_fingerprint(char *out, unsigned n)
{
    char model[128] = "unknown cpu", line[256]; int cpus = 0;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "processor", 9)) cpus++;
            if (model[0] == 'u' && !strncmp(line, "model name", 10)) {
                const char *c = strchr(line, ':'); if (c) { snprintf(model, sizeof model, "%s", c + 2); char *e = strchr(model, '\n'); if (e) *e = 0; }
            }
        }
        fclose(f);
    }
    snprintf(out, n, "%s x%d", model, cpus);
}
