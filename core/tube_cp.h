/* The in-process Tube co-processor.
 *
 * On the desktop the Tube child is a separate process on a pty (io.c). A
 * bare-metal Pi has no processes, so there the co-processor is the same
 * interpreter compiled INTO the kernel and run on a core of its own -- core
 * 3, the one nobody else wanted -- which makes it, quite literally, the
 * second processor. It talks to the machine through two lock-free rings:
 * bytes it prints go down one, keys the ROM sends come up the other.
 * Start/stop/alive are three atomic words. Nothing here knows about Circle
 * or SDL; the platform supplies ticks, a sleep and (on the desktop) a
 * thread -- see the bottom of this file.
 *
 * The same code builds on the desktop with -DK4510_TUBE_INPROC (the
 * interpreter on a pthread instead of a core) so the whole path can be
 * exercised by test/tubetest before a card is ever written. */
#ifndef K4510_TUBE_CP_H
#define K4510_TUBE_CP_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the machine's side (io.c, on the emulator's core/thread) ---------- */
int  tube_cp_start(int prog);              /* 1 = BBC BASIC; 0 accepted, -1 not fitted for that program */
void tube_cp_stop(void);                   /* the interpreter quits at its next trap; rings dropped once it has */
int  tube_cp_alive(void);
int  tube_cp_read(uint8_t *buf, int max);  /* bytes the co-processor wrote (its stdout); 0 = none */
void tube_cp_write(uint8_t b);             /* a byte for its keyboard */

/* ---- the co-processor's side (bbccon.c built with -DK4510_TUBE) -------- */
void  tube_cp_run(void);                   /* its main loop: waits for a start, runs the program, repeats. Never returns. */
int   tube_cp_getc(void);                  /* next keyboard byte, -1 = none */
void  tube_cp_puts(const char *s, size_t n);
int   tube_cp_printf(const char *fmt, ...);
int   tube_cp_flush(FILE *f);              /* stdout: nothing to do; a file: fflush */
int   tube_cp_killed(void);                /* the machine wrote 2 to $D803 */
void *tube_cp_ram(size_t *bytes);          /* the co-processor's flat memory, allocated once (halves until it fits) */

/* Files. The co-processor lives inside the machine's filesystem (fs/ on the
 * desktop, SD:/k4510/fs on the Pi) and must not chdir -- the working
 * directory is shared with the emulator on the Pi -- so it carries its own
 * current directory and prefixes every path. "/" is the machine's root. */
FILE *tube_cp_fopen(const char *path, const char *mode);
DIR  *tube_cp_opendir(const char *path);
int   tube_cp_remove(const char *path);
int   tube_cp_rename(const char *from, const char *to);
int   tube_cp_mkdir(const char *path, unsigned mode);
int   tube_cp_rmdir(const char *path);
int   tube_cp_chdir(const char *path);
char *tube_cp_getcwd(char *buf, size_t n);
int   tube_cp_chmod(const char *path, unsigned mode);   /* the Pi's card has no modes: 0 */

/* ---- what the platform supplies ---------------------------------------- */
unsigned tube_cp_ticks(void);              /* milliseconds, any origin */
void     tube_cp_usleep(unsigned us);      /* desktop: usleep; Pi: a spin on the system counter (core 3 has nothing else to do) */
int      tube_bbc_main(void);              /* the interpreter (bbccon.c); returns BASIC's exit code */
const char *fs_get_root(void);             /* io.c: the machine's filesystem root, as a host path */

#ifdef __cplusplus
}
#endif
#endif
