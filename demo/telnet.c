/* BMC-K4510: TELNET host port -- a terminal on the N: device ($D900).
 * Opens tcp://host:port on channel 0, sends what you type, prints what
 * arrives; ESC hangs up. Telnet option negotiation (IAC) is answered the
 * minimal way: every DO/WILL gets a WONT/DONT, so plain servers, MUDs and
 * BBSes talk, and a raw TCP echo talks best. */
#include "k4510.h"

#define NET      0xD900u
#define NET_CMD  (NET + 0)
#define NET_ST   (NET + 1)
#define NET_CHAN (NET + 2)

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);
static unsigned char rom_args(void) { return ((unsigned char (*)(void))0xFF95)(); }

static char url[96];
static unsigned char buf[256];

static void say(const char *s) { while (*s) rom_chrout(*s++); }
static unsigned char net(unsigned char cmd) { REG(NET_CMD) = cmd; return REG(NET_ST); }
static void net_send(const unsigned char *p, unsigned int n) { w32(NET + 8, (uint16_t)p); w32(NET + 12, n); net(3); }

void main(void)
{
    unsigned char n = rom_args(), i = 0, k, iac = 0;
    const char *p = *(const char **)0xF0;
    unsigned int got;
    if (!n) { say("telnet: host port  (ESC hangs up)\n"); return; }
    url[i++] = 't'; url[i++] = 'c'; url[i++] = 'p'; url[i++] = ':'; url[i++] = '/'; url[i++] = '/';
    while (*p && *p != ' ' && i < 90) url[i++] = *p++;
    while (*p == ' ') p++;
    if (!*p) { say("telnet: host port\n"); return; }
    url[i++] = ':';
    while (*p && *p != ' ' && i < 94) url[i++] = *p++;
    url[i] = 0;
    REG(NET_CHAN) = 0;
    w32(NET + 4, (uint16_t)url);
    if (REG(NET_ST) == 6) { say("telnet: no network on this host\n"); return; }
    if (net(1)) { say("telnet: cannot connect to "); say(url + 6); rom_chrout('\n'); return; }
    say("connected to "); say(url + 6); say("  (ESC hangs up)\n");
    for (;;) {
        k = rom_getin();
        if (k == 0x1B) break;
        if (k == 0x0D) { buf[0] = 13; buf[1] = 10; net_send(buf, 2); }
        else if (k == 0x89 || k == 0x7F) { buf[0] = 8; net_send(buf, 1); }
        else if (k && k < 0x80) { buf[0] = k; net_send(buf, 1); }
        w32(NET + 8, (uint16_t)buf); w32(NET + 12, sizeof buf);
        k = net(2);
        got = REG(NET + 12) | ((unsigned int)REG(NET + 13) << 8);
        for (i = 0; i < got; i++) {
            unsigned char c = buf[i];
            if (iac == 1) { iac = (c >= 251 && c <= 254) ? 2 : 0; if (iac == 2) buf[0] = 255, buf[1] = (c == 251 || c == 252) ? 254 : 252; continue; }
            if (iac == 2) { iac = 0; buf[2] = c; net_send(buf, 3); continue; }
            if (c == 255) { iac = 1; continue; }
            if (c == 13) continue;
            if (c == 10 || c == 8 || c == 9 || c >= 32) rom_chrout(c);
        }
        if (k == 4) { say("\nconnection closed by the far end\n"); break; }
        if (!got) wait_vblank();
    }
    net(4);
}
