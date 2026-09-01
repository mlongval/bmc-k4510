/* K4510: JIM in ANSI -- what the console is made of.
 *
 * Since D-11 the machine's console *is* JIM, the terminal at $DA00: every
 * byte the ROM prints goes through it.  Which means a program needs no
 * special access to drive the terminal -- it just prints, and the escape
 * sequences work.  Everything here goes out through CHROUT ($FF80), the
 * same call ECHO and DIR use.
 *
 * Its companion is PETSCII.PRG, the same screen in the other mode.
 */
#include "k4510.h"

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);

static void print(const char *s) { while (*s) rom_chrout(*s++); }
static void nl(void) { rom_chrout('\n'); }

/* ESC [ ... -- built by hand, because that is the point of the demo */
static void csi(const char *s) { rom_chrout(27); rom_chrout('['); print(s); }
static void sgr(const char *s) { csi(s); rom_chrout('m'); }
static void at(uint8_t row, uint8_t col)          /* CUP: 1-based, as VT100 counts */
{
    char b[8]; uint8_t i = 0;
    b[i++] = '0' + row / 10; b[i++] = '0' + row % 10; b[i++] = ';';
    b[i++] = '0' + col / 10; b[i++] = '0' + col % 10; b[i] = 0;
    csi(b); rom_chrout('H');
}

static void wait_key(void)
{
    sgr("2");                                    /* dim */
    print("  -- any key --");
    sgr("0");
    while (!rom_getin()) ;
}

int main(void)
{
    uint8_t i;

    rom_chrout(12);                              /* clear, the console's own way */

    sgr("1;37"); print("JIM in ANSI"); sgr("0");
    print("   the terminal at $DA00, driven through CHROUT"); nl(); nl();

    /* ---- colour ---- */
    print("The eight colours, and the eight bright ones:"); nl(); nl();
    print("  ");
    for (i = 0; i < 8; i++) { char b[3]; b[0] = '3'; b[1] = '0' + i; b[2] = 0; sgr(b); print("*** "); }
    sgr("0"); nl(); print("  ");
    for (i = 0; i < 8; i++) { char b[4]; b[0] = '9'; b[1] = '0' + i; b[2] = 0; sgr(b); print("*** "); }
    sgr("0"); nl(); nl();

    print("  ");
    for (i = 0; i < 8; i++) { char b[3]; b[0] = '4'; b[1] = '0' + i; b[2] = 0; sgr(b); print("    "); }
    sgr("0"); nl(); nl();

    /* ---- attributes ---- */
    print("Attributes:  ");
    sgr("1"); print("bold ");  sgr("0");
    sgr("7"); print(" reverse "); sgr("0");
    sgr("2"); print(" dim");   sgr("0");
    nl(); nl();

    /* ---- the DEC line-drawing set: a real box ---- */
    print("The DEC line-drawing set (ESC ( 0):"); nl(); nl();
    rom_chrout(27); rom_chrout('('); rom_chrout('0');       /* G0 := line drawing */
    print("  lqqqqqqqqqqqqqqqqqqqqqk"); nl();
    print("  x"); rom_chrout(27); rom_chrout('('); rom_chrout('B');
    sgr("1;36"); print("    a box, drawn     "); sgr("0");   /* 21: the width of the rule above */
    rom_chrout(27); rom_chrout('('); rom_chrout('0'); print("x"); nl();
    print("  mqqqqqqqqqqqqqqqqqqqqqj"); nl();
    rom_chrout(27); rom_chrout('('); rom_chrout('B');        /* G0 := ASCII again */
    nl();

    wait_key();
    rom_chrout(12);

    /* ---- absolute positioning ---- */
    sgr("1;37"); print("Cursor positioning"); sgr("0"); nl();
    print("ESC[row;colH puts a character anywhere:"); nl();
    for (i = 0; i < 10; i++) {
        char b[3]; b[0] = '3'; b[1] = '1' + (i & 5); b[2] = 0;
        sgr(b);
        at((uint8_t)(6 + i), (uint8_t)(6 + i * 3));
        print("K4510");
    }
    sgr("0");
    at(20, 1);

    /* ---- a scroll region: the VT100 trick the status bands use ---- */
    print("A scroll region (DECSTBM): rows 22-26 scroll, the rest stays put."); nl();
    csi("22;26r");                               /* set the region */
    at(22, 1);
    for (i = 0; i < 14; i++) {
        char b[3]; b[0] = '9'; b[1] = '0' + (i & 7); b[2] = 0;
        sgr(b); print("    scrolling inside the region -- line "); rom_chrout('A' + i); nl();
    }
    sgr("0");
    csi("r");                                    /* the whole screen again */
    at(28, 1);
    wait_key();

    rom_chrout(12);
    print("PETSCII.PRG is the same terminal in the other mode."); nl();
    return 0;
}
