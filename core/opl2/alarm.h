/* K4510 shim for VICE's alarm system.  The OPL2 uses exactly two alarms --
 * its Timer A and Timer B, which set the status register's overflow flags a
 * player can poll -- so the shim is two of them per chip and a poll, rather
 * than VICE's general machinery.  core/opl2.c fires them as the clock passes.
 */
#ifndef K4510_VICE_ALARM_H
#define K4510_VICE_ALARM_H
#include "types.h"
typedef void (*alarm_callback_t)(CLOCK offset, void *data);
typedef struct alarm_context_s alarm_context_t;
typedef struct alarm_s {
    CLOCK when;                 /* the clock value it is due at */
    int   set;                  /* armed */
    alarm_callback_t cb;
    void *data;
} alarm_t;
extern alarm_context_t *maincpu_alarm_context;   /* unused; VICE names one */
alarm_t *alarm_new(alarm_context_t *ctx, const char *name, alarm_callback_t cb, void *data);
void     alarm_destroy(alarm_t *a);
void     alarm_set(alarm_t *a, uint32_t when);
void     alarm_unset(alarm_t *a);
void     alarm_poll(uint32_t now);   /* K4510: fire whatever is due (core/opl2.c) */
#endif
