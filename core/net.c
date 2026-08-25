/* The network: the Meatloaf rule and the N: device. See net.h. */
#include "net.h"
#include "mem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

int net_is_url(const char *name)
{
    return !strncasecmp(name, "http://", 7) || !strncasecmp(name, "https://", 8);
}

#ifdef K4510_PI
/* Not fitted yet: URLs are ordinary (absent) names and the device says so. */
int net_fetch_file(const char *url, char *path, size_t max) { (void) url; (void) path; (void) max; return -1; }
void net_reset(void) {}
uint8_t net_read(uint8_t reg) { return reg == 1 ? 6 : 0xFF; }
void net_write(uint8_t reg, uint8_t v) { (void) reg; (void) v; }
#else

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netdb.h>
#include <poll.h>

/* ---- curl, without a shell: argv straight to execvp ---------------------- */
static pid_t spawn_curl(const char *url, const char *outfile, int *outfd)
{
    int p[2] = { -1, -1 };
    if (outfd && pipe(p)) return -1;
    pid_t pid = fork();
    if (pid < 0) { if (outfd) { close(p[0]); close(p[1]); } return -1; }
    if (pid == 0) {
        if (outfd) { dup2(p[1], 1); close(p[0]); close(p[1]); }
        int nul = open("/dev/null", O_RDWR); if (nul >= 0) { dup2(nul, 0); dup2(nul, 2); if (nul > 2) close(nul); }
        if (outfile) execlp("curl", "curl", "-sSL", "--max-time", "120", "--fail", "-o", outfile, "--", url, (char *) NULL);
        else         execlp("curl", "curl", "-sSL", "--max-time", "120", "--fail", "--", url, (char *) NULL);
        _exit(127);
    }
    if (outfd) { close(p[1]); *outfd = p[0]; }
    return pid;
}

int net_fetch_file(const char *url, char *path, size_t max)
{
    char tmpl[] = "/tmp/k4510-net-XXXXXX"; int fd, st = 1;
    if (!net_is_url(url)) return -1;
    if ((fd = mkstemp(tmpl)) < 0) return -1;
    close(fd);
    pid_t pid = spawn_curl(url, tmpl, NULL);
    if (pid > 0) waitpid(pid, &st, 0);
    if (pid <= 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) { unlink(tmpl); return -1; }
    snprintf(path, max, "%s", tmpl);
    return 0;
}

/* ---- the N: device ------------------------------------------------------- */
enum { CH_NONE, CH_TCP, CH_HTTP };
static struct chan { int type, fd, eof; pid_t pid; } ch[4];
static uint8_t net_reg[0x14];
static uint32_t rd32r(int off) { return net_reg[off] | (net_reg[off + 1] << 8) | (net_reg[off + 2] << 16) | ((uint32_t) net_reg[off + 3] << 24); }
static void wr32r(int off, uint32_t v) { for (int i = 0; i < 4; i++) net_reg[off + i] = (v >> (8 * i)) & 0xFF; }

static void chan_close(struct chan *c)
{
    if (c->fd >= 0) close(c->fd);
    if (c->pid > 0) { kill(c->pid, SIGTERM); waitpid(c->pid, NULL, 0); }
    c->type = CH_NONE; c->fd = -1; c->pid = 0; c->eof = 0;
}
void net_reset(void)
{
    for (int i = 0; i < 4; i++) { if (ch[i].type) chan_close(&ch[i]); ch[i].fd = -1; }
    memset(net_reg, 0, sizeof net_reg);
}
static int guest_str(uint32_t p, char *s, size_t max)
{
    size_t n = 0; p &= K4510_PHYS_MASK;
    for (; n < max - 1; n++) { s[n] = (char) k4510_ram[(p + n) & K4510_PHYS_MASK]; if (!s[n]) break; }
    if (n >= max - 1) return 5;
    return 0;
}
static int open_tcp(struct chan *c, const char *spec)      /* host:port */
{
    char host[256]; const char *colon = strrchr(spec, ':'); struct addrinfo hints, *res, *ai;
    if (!colon || colon == spec || !colon[1]) return 1;
    snprintf(host, sizeof host, "%.*s", (int) (colon - spec), spec);
    memset(&hints, 0, sizeof hints); hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, colon + 1, &hints, &res)) return 1;
    for (ai = res; ai; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (!connect(fd, ai->ai_addr, ai->ai_addrlen)) { fcntl(fd, F_SETFL, O_NONBLOCK); c->type = CH_TCP; c->fd = fd; freeaddrinfo(res); return 0; }
        close(fd);
    }
    freeaddrinfo(res);
    return 1;
}
static int open_http(struct chan *c, const char *url)
{
    int fd; pid_t pid = spawn_curl(url, NULL, &fd);
    if (pid <= 0) return 2;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    c->type = CH_HTTP; c->fd = fd; c->pid = pid;
    return 0;
}
static int avail(struct chan *c)                           /* bytes waiting; -1 once closed and drained */
{
    int n = 0; struct pollfd pf = { c->fd, POLLIN, 0 };
    if (c->fd < 0 || c->eof) return -1;
    if (ioctl(c->fd, FIONREAD, &n) == 0 && n > 0) return n;
    if (poll(&pf, 1, 0) > 0) {
        if (c->type == CH_TCP) { char b; if (recv(c->fd, &b, 1, MSG_PEEK | MSG_DONTWAIT) == 0) c->eof = 1; }
        else if (pf.revents & POLLHUP) c->eof = 1;              /* a pipe: the writer (curl) has gone */
    }
    return c->eof ? -1 : 0;
}
static void net_run(uint8_t cmd)
{
    struct chan *c = &ch[net_reg[2] & 3]; int st = 0;
    uint32_t addr = rd32r(8) & K4510_PHYS_MASK, len = rd32r(12);
    char url[512];
    switch (cmd) {
    case 1: {
        if ((st = guest_str(rd32r(4), url, sizeof url))) break;
        if (c->type) chan_close(c);
        wr32r(0x10, 0xFFFFFFFFu);
        if (!strncasecmp(url, "tcp://", 6)) st = open_tcp(c, url + 6);
        else if (net_is_url(url)) st = open_http(c, url);
        else st = 1;
        break; }
    case 2: {
        uint32_t done = 0;
        if (!c->type) { st = 2; break; }
        while (done < len) {
            ssize_t r = read(c->fd, url, len - done < sizeof url ? len - done : sizeof url);
            if (r > 0) { for (ssize_t i = 0; i < r; i++) k4510_ram[(addr + done + i) & K4510_PHYS_MASK] = (uint8_t) url[i]; done += (uint32_t) r; continue; }
            if (r == 0) c->eof = 1;
            break;
        }
        wr32r(12, done);
        if (!done && c->eof) st = 4;
        break; }
    case 3: {
        if (c->type != CH_TCP) { st = c->type ? 3 : 2; break; }
        uint32_t done = 0;
        while (done < len) {
            size_t n = len - done < sizeof url ? len - done : sizeof url;
            for (size_t i = 0; i < n; i++) url[i] = (char) k4510_ram[(addr + done + i) & K4510_PHYS_MASK];
            ssize_t w = send(c->fd, url, n, MSG_NOSIGNAL);
            if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { struct pollfd pf = { c->fd, POLLOUT, 0 }; poll(&pf, 1, 1000); continue; }
            if (w <= 0) { st = 4; break; }
            done += (uint32_t) w;
        }
        wr32r(12, done);
        break; }
    case 4: if (c->type) chan_close(c); break;
    case 5: {
        if (!c->type) { st = 2; wr32r(0x10, 0); break; }
        int n = avail(c);
        wr32r(0x10, n < 0 ? 0 : (uint32_t) n);
        if (n < 0) st = 4;
        break; }
    case 6: {
        char path[64]; FILE *f; uint32_t done = 0; int b;
        if ((st = guest_str(rd32r(4), url, sizeof url))) break;
        if (net_fetch_file(url, path, sizeof path)) { st = 1; break; }
        f = fopen(path, "rb"); unlink(path);
        if (!f) { st = 2; break; }
        { struct stat sb; wr32r(0x10, !fstat(fileno(f), &sb) ? (uint32_t) sb.st_size : 0); }
        while (done < len && (b = fgetc(f)) != EOF) k4510_ram[(addr + done++) & K4510_PHYS_MASK] = (uint8_t) b;
        fclose(f);
        wr32r(12, done);
        break; }
    default: st = 3;
    }
    net_reg[1] = (uint8_t) st;
}
uint8_t net_read(uint8_t reg) { return reg < sizeof net_reg ? net_reg[reg] : 0xFF; }
void net_write(uint8_t reg, uint8_t v)
{
    if (reg == 0) { net_run(v); return; }
    if (reg < sizeof net_reg) net_reg[reg] = v;
}
#endif
