/* K4510: JIM in PETSCII -- the other way an 8-bit machine talked to its screen.
 *
 * PETSCII is not a protocol.  It is a set of control codes and a character
 * set, so JIM implements it as a second dispatch beside the ANSI one: the
 * colours land in the same fg/bg, reverse in the same attribute, and the
 * printing goes through the same code.  One renderer, two languages.
 *
 * The mode is FLAGS bit 2 at $DA0E.  This program writes straight to JIM's
 * data register rather than through CHROUT, because the ROM console speaks
 * ANSI and folds a few bytes on the way past ($0D, $08, $0C); PETSCII wants
 * them raw.  It puts the terminal back into ANSI before it returns -- leave
 * it in PETSCII and the shell would come back to a screen it cannot drive.
 *
 * A NOTE ON GLYPHS.  The letters and punctuation are right in any font.  The
 * graphics characters ($A0-$FF) are screen codes, and they only look like
 * PETSCII with a PETSCII chargen selected: F7 -> Screen font -> BESCII (or a
 * chargen.bin of your own in /SYSTEM).  With the machine's ASCII font they
 * come out as the wrong glyphs, which is the honest result -- the mode is
 * working, the font is not a PETSCII one.
 *
 * Its companion is ANSIDEMO.PRG, the same screen in the other mode.
 */
#include "k4510.h"

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);

#define TERM      0xDA00u
#define T_DATA    REG(TERM + 0x00)
#define T_FLAGS   REG(TERM + 0x0E)

/* PETSCII control codes */
#define P_WHITE   0x05
#define P_RETURN  0x0D
#define P_LOWER   0x0E
#define P_RED     0x1C
#define P_GREEN   0x1E
#define P_BLUE    0x1F
#define P_HOME    0x13
#define P_RVSON   0x12
#define P_RVSOFF  0x92
#define P_CLR     0x93
#define P_DOWN    0x11
#define P_RIGHT   0x1D
#define P_UPPER   0x8E
#define P_ORANGE  0x81
#define P_BLACK   0x90
#define P_YELLOW  0x9E
#define P_CYAN    0x9F
#define P_PURPLE  0x9C

/* the sixteen, in the C64's own palette order */
static const unsigned char cols[16] = {
    P_BLACK, P_WHITE, P_RED, P_CYAN, P_PURPLE, P_GREEN, P_BLUE, P_YELLOW,
    P_ORANGE, 0x95 /*brown*/, 0x96 /*lt red*/, 0x97 /*dk grey*/,
    0x98 /*grey*/, 0x99 /*lt green*/, 0x9A /*lt blue*/, 0x9B /*lt grey*/
};

static void put(unsigned char c) { T_DATA = c; }
static void print(const char *s) { while (*s) put(*s++); }
static void nl(void) { put(P_RETURN); }

static void wait_key(void)
{
    print(P_RETURN ? "" : "");
    nl();
    put(0x9B); print("  PRESS A KEY");
    while (!rom_getin()) ;
}

int main(void)
{
    unsigned char i;

    T_FLAGS |= 4;                      /* PETSCII on */
    put(P_CLR);

    put(P_WHITE); put(P_RVSON);
    print("  JIM IN PETSCII  ");
    put(P_RVSOFF); nl(); nl();

    put(0x9B);
    print("THE SAME TERMINAL AS ANSIDEMO.PRG,"); nl();
    print("SPEAKING THE OTHER LANGUAGE."); nl(); nl();

    /* ---- the sixteen colours ---- */
    put(P_WHITE); print("THE SIXTEEN COLOUR CODES:"); nl(); nl();
    print("  ");
    for (i = 0; i < 16; i++) { put(cols[i]); put(P_RVSON); print("  "); put(P_RVSOFF); }
    nl(); nl();

    /* ---- reverse, the CBM way ---- */
    put(P_WHITE); print("REVERSE VIDEO IS A CONTROL CODE, NOT AN"); nl();
    print("ESCAPE SEQUENCE:  ");
    put(P_YELLOW); put(P_RVSON); print(" RVS ON "); put(P_RVSOFF);
    put(P_WHITE); print("  AND OFF."); nl(); nl();

    /* ---- cursor codes ---- */
    put(P_WHITE); print("CURSOR CODES MOVE WITHOUT PRINTING:"); nl(); nl();
    print("  ");
    put(P_GREEN);
    for (i = 0; i < 10; i++) { print("*"); put(P_RIGHT); }
    nl(); nl();

    /* ---- the graphics half ---- */
    put(P_WHITE); print("AND THE GRAPHICS HALF ($A0-$FF).  THESE"); nl();
    print("WANT A PETSCII CHARGEN -- F7, SCREEN FONT:"); nl(); nl();
    put(P_CYAN); print("  ");
    for (i = 0xA0; i < 0xC0; i++) put(i);
    nl(); print("  ");
    for (i = 0xC0; i < 0xE0; i++) put(i);
    nl(); nl();

    put(P_LOWER);
    put(0x9B); print("($0E selects the lower-case set, $8E the");
    nl(); print("upper-case and graphics one.)");
    put(P_UPPER);
    nl();

    wait_key();

    T_FLAGS &= (unsigned char)~4;      /* ANSI again, or the shell comes back to a screen it cannot drive */
    rom_chrout(12);
    return 0;
}
