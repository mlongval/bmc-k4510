/* MATH unit at $D700: integer mul/div (MEGA65 layout) and the float registers. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/mem.h"
#include "../core/io.h"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void w32(uint16_t a, uint32_t v) { for (int i = 0; i < 4; i++) io_write(a + i, (uint8_t)(v >> (8 * i))); }
static uint32_t r32(uint16_t a) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)io_read(a + i) << (8 * i); return v; }
static void fset(int n, float f) { uint32_t u; memcpy(&u, &f, 4); w32(IO_MATH + n * 4, u); }
static float fget(int n) { uint32_t u = r32(IO_MATH + n * 4); float f; memcpy(&f, &u, 4); return f; }
static void fop(int op, int d, int s) { io_write(IO_MATH + 0x21, (uint8_t)((d << 4) | s)); io_write(IO_MATH + 0x20, (uint8_t)op); }
int main(void)
{
    mem_init(); io_reset();
    w32(IO_MATH + 0x70, 123456789u); w32(IO_MATH + 0x74, 1000u);
    CHECK(r32(IO_MATH + 0x78) == 3197704712u && r32(IO_MATH + 0x7C) == 28u, "MULTOUT = 123456789 * 1000 (64-bit)");
    CHECK(r32(IO_MATH + 0x6C) == 123456u, "DIVOUT integer part = 123456");
    CHECK((r32(IO_MATH + 0x68) >> 24) == 0xC9, "DIVOUT fraction .789 -> top byte $C9");
    w32(IO_MATH + 0x74, 0); CHECK(r32(IO_MATH + 0x6C) == 0xFFFFFFFFu, "divide by zero gives all ones");
    fset(0, 1.5f); fset(1, 2.25f); fop(MATH_ADD, 0, 1); CHECK(fget(0) == 3.75f, "ADD in place: F0 = F0 + F1 = 3.75");
    fop(MATH_MUL, 0, 1); CHECK(fabsf(fget(0) - 8.4375f) < 1e-6f, "MUL: 3.75 * 2.25");
    fset(2, 16.0f); fop(MATH_SQRT, 3, 2); CHECK(fget(3) == 4.0f, "SQRT F3 = sqrt(F2)");
    fset(4, 0.0f); fop(MATH_COS, 4, 4); CHECK(fget(4) == 1.0f, "COS in place");
    fset(5, -2.5f); fop(MATH_CMP, 5, 4); CHECK((io_read(IO_MATH + 0x22) & 3) == 2, "CMP -2.5 < 1.0 sets negative, registers kept");
    CHECK(fget(5) == -2.5f, "CMP leaves F5 alone");
    w32(IO_MATH + 0x24, (uint32_t)-1234); fop(MATH_ITOF, 6, 0); CHECK(fget(6) == -1234.0f, "ITOF");
    fset(7, 3.99f); fop(MATH_FTOI, 0, 7); CHECK((int32_t)r32(IO_MATH + 0x24) == 3, "FTOI truncates 3.99 -> 3");
    fset(1, 0.0f); fop(MATH_DIV, 0, 1); CHECK(io_read(IO_MATH + 0x22) & 4, "x/0 flags inf");
    /* math list: F0 = 1.5; loop 10 times: F0 = F0 * 2, stop when F0 >= 1000 (via FTOI + STOPFIGE... too big for a byte:
       use CMP against F1 = 1000 and STOPPOS instead); count how many doublings it took */
    { uint8_t list[] = { ML_LDF, 0x00, 0,0,0xC0,0x3F,          /* F0 = 1.5 */
                         ML_LDF, 0x10, 0,0,0x7A,0x44,          /* F1 = 1000.0 */
                         MATH_ADD, 0x00,                       /* loop: F0 += F0 */
                         MATH_CMP, 0x01, ML_STOPPOS, 0,        /* stop if F0 - F1 >= 0 */
                         ML_DJNZ, (uint8_t)-4,                 /* back 4 ops (from the op after DJNZ) to the ADD */
                         ML_END, 0 };
      for (unsigned i = 0; i < sizeof list; i++) mem_poke(0x3000 + i, list[i]);
      w32(IO_MATH + 0x28, 0x3000); io_write(IO_MATH + 0x2E, 20); io_write(IO_MATH + 0x2F, 0);
      io_write(IO_MATH + 0x2C, 1);
      CHECK(io_read(IO_MATH + 0x2D) == 1, "math list: stopped by STOPPOS");
      CHECK(fget(0) == 1536.0f, "math list: 1.5 doubled until >= 1000 is 1536");
      CHECK(io_read(IO_MATH + 0x2E) == 20 - 9, "math list: DJNZ ran 9 times before the stop on the 10th doubling");
      io_write(IO_MATH + 0x2E, 3); io_write(IO_MATH + 0x2C, 1);
      CHECK(io_read(IO_MATH + 0x2D) == 0 && fget(0) == 12.0f, "math list: runs out of count -> END, F0 = 1.5 * 8"); }
    /* LDMS: Microsoft float 3.5 = exponent $82, mantissa $E0 00 00 (0.875 * 2^2), positive */
    { mem_poke(0x4000, 0x82); mem_poke(0x4001, 0x60); mem_poke(0x4002, 0); mem_poke(0x4003, 0);     /* sign bit clear, leading 1 implied: $60 -> 1.11 */
      uint8_t list[] = { ML_LDMS, 0x20, 0x00, 0x40, 0x00, 0x00, ML_END, 0 };
      for (unsigned i = 0; i < sizeof list; i++) mem_poke(0x3100 + i, list[i]);
      w32(IO_MATH + 0x28, 0x3100); io_write(IO_MATH + 0x2C, 1);
      CHECK(fget(2) == 3.5f, "LDMS converts a Microsoft-format 3.5 (got %g)", fget(2));
      mem_poke(0x4001, 0xE0); io_write(IO_MATH + 0x2C, 1);
      CHECK(fget(2) == -3.5f, "LDMS honours the sign bit"); }
    printf(fails ? "\n%d FAILED\n" : "ALL OK\n", fails); return fails != 0;
}
