/* The machine's microsecond clock -- see vice_clk.h.  Two vendored pieces of
 * VICE age against it, and the OPL2's timers are due on it, so the advance
 * fires the alarms as it goes. */
#include "vice_clk.h"
#include "opl2/alarm.h"
uint32_t maincpu_clk;
void vice_clk_reset(void) { maincpu_clk = 0; }
void vice_clk_advance(uint32_t us)
{
    if (!us) return;
    maincpu_clk += us;
    alarm_poll(maincpu_clk);
}
