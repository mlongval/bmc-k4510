// The network on the Pi: Circle's TCP/IP stack, reached from the emulator's
// core through the shim's marshalling. Circle's subsystems are core-0-only
// (docs/CORE-SPLIT.md), so every socket, DNS and HTTP call here runs inside
// SDL2Circle_CallOn0; the waits that would block -- a UDP reply, a byte on a
// TCP socket -- are polled non-blocking from core 1 with a millisecond nap
// between tries, so core 0's servo (USB, audio, the card) is never held.
//
// The stack itself is the one circle-libsdl2 constructs but never starts
// (src/network.cpp): k4510_net_start() on core 0 starts it -- Ethernet, DHCP
// -- and plat_net_ready() answers once an address is bound. No cable, no
// address: the N: device reads 6, URLs are absent names, nothing halts.
// HTTPS: no TLS is built into this kernel; it answers 6 (unsupported).
#include "../core/net_plat.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_circle.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/net/dnsclient.h>
#include <circle/net/httpclient.h>
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

static const char From[] = "k4510-net";
static bool s_started;

template <typename F> static void on0(F &f) { SDL2Circle_CallOn0([](void *p) { (*static_cast<F *>(p))(); }, &f); }

extern "C" void k4510_net_start(void)          // core 0, once the shim has brought USB up
{
    CNetSubSystem *net = CNetSubSystem::Get(); // constructed (never started) by the shim's ArmCoreRuntime
    if (net && net->Initialize(FALSE)) { s_started = true; SDL2Circle_Log(From, SDL2CIRCLE_LOG_NOTICE, "Ethernet up, DHCP running"); }
    else SDL2Circle_Log(From, SDL2CIRCLE_LOG_NOTICE, "no network device: the N: device is not fitted");
}
extern "C" int plat_net_ready(void) { return s_started && CNetSubSystem::Get()->IsRunning(); }
extern "C" unsigned plat_ticks(void) { return SDL_GetTicks(); }

static bool resolve(const char *host, CIPAddress &ip)   // on core 0
{
    unsigned a, b, c, d;
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) { u8 q[4] = { (u8) a, (u8) b, (u8) c, (u8) d }; ip.Set(q); return true; }
    CDNSClient dns(CNetSubSystem::Get());
    return dns.Resolve(host, &ip) != 0;
}

// ---- sockets: a small table, handles are indices --------------------------
struct sock { CSocket *s; bool tcp; u8 stash[2048]; int stash_n; bool closed; };
static sock socks[8];
static int alloc_sock(void) { for (int i = 0; i < 8; i++) if (!socks[i].s) return i; return -1; }

static int open_sock(const char *host, int port, bool tcp)
{
    int h = alloc_sock(); if (h < 0) return -1;
    int rc = -1;
    auto f = [&]() {
        CIPAddress ip; if (!resolve(host, ip)) return;
        CSocket *s = new CSocket(CNetSubSystem::Get(), tcp ? IPPROTO_TCP : IPPROTO_UDP);
        if (s->Connect(ip, (u16) port) < 0) { delete s; return; }
        socks[h].s = s; socks[h].tcp = tcp; socks[h].stash_n = 0; socks[h].closed = false; rc = 0;
    };
    on0(f);
    return rc < 0 ? -1 : h;
}
static void close_sock(int h)
{
    if (h < 0 || h >= 8 || !socks[h].s) return;
    auto f = [&]() { delete socks[h].s; socks[h].s = nullptr; };
    on0(f);
}
extern "C" int  plat_udp_open(const char *host, int port) { return open_sock(host, port, false); }
extern "C" void plat_udp_close(int h) { close_sock(h); }
extern "C" int  plat_udp_send(int h, const void *buf, int n)
{
    int r = -1; if (h < 0 || !socks[h].s) return -1;
    auto f = [&]() { r = socks[h].s->Send(buf, (unsigned) n, 0); };
    on0(f);
    return r;
}
extern "C" int plat_udp_recv(int h, void *buf, int max, int timeout_ms)
{
    unsigned end = SDL_GetTicks() + (unsigned) timeout_ms; int r = 0;
    if (h < 0 || !socks[h].s) return -1;
    for (;;) {
        auto f = [&]() { r = socks[h].s->Receive(buf, (unsigned) max, MSG_DONTWAIT); };
        on0(f);
        if (r > 0) return r;
        if (r < 0) return -1;
        if ((int)(SDL_GetTicks() - end) >= 0) return 0;
        SDL_Delay(1);
    }
}
extern "C" int  plat_tcp_connect(const char *host, int port) { return open_sock(host, port, true); }
extern "C" void plat_tcp_close(int h) { close_sock(h); }
extern "C" int  plat_tcp_send(int h, const void *buf, int n)
{
    int r = -1; if (h < 0 || !socks[h].s) return -1;
    auto f = [&]() { r = socks[h].s->Send(buf, (unsigned) n, 0); };
    on0(f);
    return r < 0 ? -1 : n;
}
static void fill_stash(int h)                  // pull what is there into the stash, non-blocking
{
    sock &k = socks[h];
    if (k.closed || k.stash_n == (int) sizeof k.stash) return;
    int r = 0;
    auto f = [&]() { r = k.s->Receive(k.stash + k.stash_n, sizeof k.stash - (unsigned) k.stash_n, MSG_DONTWAIT); };
    on0(f);
    if (r > 0) k.stash_n += r;
    else if (r < 0) k.closed = true;
}
extern "C" int plat_tcp_recv(int h, void *buf, int max)
{
    if (h < 0 || !socks[h].s) return -1;
    sock &k = socks[h];
    if (!k.stash_n) fill_stash(h);
    if (k.stash_n) { int n = k.stash_n < max ? k.stash_n : max; memcpy(buf, k.stash, (size_t) n); memmove(k.stash, k.stash + n, (size_t)(k.stash_n - n)); k.stash_n -= n; return n; }
    return k.closed ? -1 : 0;
}
extern "C" int plat_tcp_avail(int h)
{
    if (h < 0 || !socks[h].s) return -1;
    fill_stash(h);
    if (socks[h].stash_n) return socks[h].stash_n;
    return socks[h].closed ? -1 : 0;
}

// ---- HTTP: Circle's client, the whole body into a buffer ------------------
extern "C" int plat_http_fetch(const char *url, uint8_t **buf, uint32_t *len)
{
    char host[128], path[512]; int port = 80; const char *p, *e;
    if (!strncasecmp(url, "https://", 8)) return 6;
    if (strncasecmp(url, "http://", 7)) return 1;
    p = url + 7; e = strchr(p, '/'); if (!e) e = p + strlen(p);
    if ((size_t)(e - p) >= sizeof host) return 5;
    memcpy(host, p, (size_t)(e - p)); host[e - p] = 0;
    if (char *c = strrchr(host, ':')) { port = atoi(c + 1); *c = 0; }
    snprintf(path, sizeof path, "%s", *e ? e : "/");
    const unsigned cap = 8u << 20;             // 8 MB: Circle's client wants the whole buffer up front
    u8 *b = (u8 *) malloc(cap); if (!b) return 2;
    int rc = 2; unsigned n = cap;
    auto f = [&]() {
        CIPAddress ip; if (!resolve(host, ip)) { rc = 1; return; }
        CHTTPClient client(CNetSubSystem::Get(), ip, (u16) port, host);
        THTTPStatus st = client.Get(path, b, &n);
        rc = st == HTTPOK ? 0 : st == HTTPNotFound ? 1 : 2;
    };
    on0(f);
    if (rc) { free(b); return rc; }
    *buf = (uint8_t *) realloc(b, n ? n : 1); *len = n;
    return 0;
}
