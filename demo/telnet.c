/* BMC-K4510: TELNET host port -- a terminal on the N: device ($D900).
 * Opens tcp://host:port on channel 0, sends what you type, and what
 * arrives goes to JIM, the terminal ($DA00): a VT100/ANSI in hardware, so
 * BBSes get their ANSI art and colours, and cursor and function keys go
 * out as VT sequences. F12 hangs up (Escape is a key the far end wants).
 * Telnet option negotiation (IAC): the server is told the terminal type
 * (ANSI) and the window size (JIM's columns and rows, NAWS), so a BBS
 * lays its screens out for this screen; every other DO/WILL gets a
 * WONT/DONT, so plain servers, MUDs and a raw TCP echo talk too. */
#include "k4510.h"

#define NET      0xD900u
#define NET_CMD  (NET + 0)
#define NET_ST   (NET + 1)
#define NET_CHAN (NET + 2)
#define TERM     0xDA00u

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);
static unsigned char rom_args(void) { return ((unsigned char (*)(void))0xFF95)(); }

static char url[96];
static unsigned char buf[256], rep[12], sb[16], sbn, cmd;

static void say(const char *s) { while (*s) REG(TERM) = *s++; }
static unsigned char net(unsigned char cmd) { REG(NET_CMD) = cmd; return REG(NET_ST); }
static void net_send(const unsigned char *p, unsigned int n) { w32(NET + 8, (uint16_t)p); w32(NET + 12, n); net(3); }

void main(void)
{
    unsigned char n = rom_args(), i = 0, k, iac = 0;
    const char *p = *(const char **)0xF0;
    unsigned int got, j;                              /* j indexes the 256-byte read buffer: a byte would wrap on a full one */
    REG(TERM + 4) = 1;                                    /* JIM: defaults, home... */
    REG(TERM + 9) = 0;                                    /* ...at the console's line (run_at handed the row over; the column is 0) */
    if (!n) { say("telnet: host port  (F12 hangs up)\r\n"); return; }
    url[i++] = 't'; url[i++] = 'c'; url[i++] = 'p'; url[i++] = ':'; url[i++] = '/'; url[i++] = '/';
    while (*p && *p != ' ' && i < 90) url[i++] = *p++;
    while (*p == ' ') p++;
    if (!*p) { say("telnet: host port\r\n"); return; }
    url[i++] = ':';
    while (*p && *p != ' ' && i < 94) url[i++] = *p++;
    url[i] = 0;
    REG(NET_CHAN) = 0;
    w32(NET + 4, (uint16_t)url);
    if (REG(NET_ST) == 6) { say("telnet: no network on this host\r\n"); return; }
    if (net(1)) { say("telnet: cannot connect to "); say(url + 6); say("\r\n"); return; }
    say("connected to "); say(url + 6); say("  (F12 hangs up)\r\n");
    REG(TERM + 0x0E) = 1;                                 /* JIM's cursor */
    for (;;) {
        k = rom_getin();
        if (k == 0x9B) break;                             /* F12 */
        if (k == 0x0D) { buf[0] = 13; buf[1] = 10; net_send(buf, 2); }
        else if (k) {                                     /* through JIM: arrows and F-keys become VT sequences */
            REG(TERM + 3) = k;
            for (i = 0; (REG(TERM + 1) & 0x80) && i < 16; i++) buf[i] = REG(TERM + 2);
            if (i) net_send(buf, i);
        }
        w32(NET + 8, (uint16_t)buf); w32(NET + 12, sizeof buf);
        k = net(2);
        got = REG(NET + 12) | ((unsigned int)REG(NET + 13) << 8);
        for (j = 0; j < got; j++) {
            unsigned char c = buf[j];
            if (iac == 1) {                               /* after IAC */
                if (c == 255) { iac = 0; REG(TERM) = 255; continue; }
                if (c == 250) { iac = 3; sbn = 0; continue; }   /* SB: a subnegotiation follows */
                iac = (c >= 251 && c <= 254) ? 2 : 0; cmd = c; continue;
            }
            if (iac == 2) {                               /* DO/DONT/WILL/WONT + option */
                iac = 0;
                if (cmd == 253 && c == 31) {              /* DO NAWS: WILL, then the size */
                    rep[0] = 255; rep[1] = 251; rep[2] = 31; net_send(rep, 3);
                    rep[0] = 255; rep[1] = 250; rep[2] = 31; rep[3] = 0; rep[4] = REG(TERM + 5); rep[5] = 0; rep[6] = REG(TERM + 6); rep[7] = 255; rep[8] = 240;
                    net_send(rep, 9);
                } else if (cmd == 253 && c == 24) { rep[0] = 255; rep[1] = 251; rep[2] = 24; net_send(rep, 3); }   /* DO TTYPE: WILL; the server asks next */
                else if (c == 0 || c == 1 || c == 3) {   /* BINARY, ECHO, SGA: agreed, not refused -- a BBS echoes for
                                                         * us, runs character-at-a-time (a Major BBS hangs up
                                                         * without SGA) and needs 8 bits for its CP437 art */
                    rep[0] = 255; rep[2] = c;
                    rep[1] = (cmd == 251) ? 253 : (cmd == 252) ? 254 : (cmd == 253) ? 251 : 252;
                    net_send(rep, 3);
                }
                else { rep[0] = 255; rep[1] = (cmd == 251 || cmd == 252) ? 254 : 252; rep[2] = c; net_send(rep, 3); }
                continue;
            }
            if (iac == 3) {                               /* inside SB ... IAC SE */
                if (c == 255) { iac = 4; continue; }
                if (sbn < sizeof sb) sb[sbn++] = c;
                continue;
            }
            if (iac == 4) {
                if (c == 240) {                           /* SE: TTYPE SEND -> IS "ANSI" */
                    iac = 0;
                    if (sbn >= 2 && sb[0] == 24 && sb[1] == 1) { rep[0] = 255; rep[1] = 250; rep[2] = 24; rep[3] = 0; rep[4] = 'A'; rep[5] = 'N'; rep[6] = 'S'; rep[7] = 'I'; rep[8] = 255; rep[9] = 240; net_send(rep, 10); }
                    continue;
                }
                if (sbn < sizeof sb) sb[sbn++] = c; iac = 3; continue;
            }
            if (c == 255) { iac = 1; continue; }
            REG(TERM) = c;
        }
        for (i = 0; (REG(TERM + 1) & 0x80) && i < 16; i++) buf[i] = REG(TERM + 2);   /* JIM's replies (a BBS asking where the cursor is) */
        if (i) net_send(buf, i);
        if (k == 4) { say("\r\nconnection closed by the far end\r\n"); break; }
        if (!got) wait_vblank();
    }
    net(4);
    REG(TERM + 0x0E) = 0;
}
