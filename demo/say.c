/* BMC-K4510: SAY -- the smallest possible disk command, and the proof of
 * the REXX rule: an unknown shell word runs name.prg from disk, and the
 * program reads what followed it with the ARGS system call ($FF95:
 * $F0/$F1 -> the tail, A = its length). SAY prints its arguments. */
#include "k4510.h"

void __fastcall__ rom_chrout(unsigned char c);

static unsigned char rom_args(void) { return ((unsigned char (*)(void))0xFF95)(); }

void main(void)
{
    unsigned char n = rom_args();
    const char *p = *(const char **)0xF0;
    if (!n) { const char *u = "say: nothing to say (SAY <words>)"; while (*u) rom_chrout(*u++); }
    else while (*p) rom_chrout(*p++);
    rom_chrout('\n');
}
