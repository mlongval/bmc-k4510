/* The settings registry. See settings.h. */
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *font_names[]  = { "kernel8", "unscii", "open-roms", "PXLfont", "C64 chargen" };   /* the last one is renamed by the host if /SYSTEM/chargen.bin is absent */
static const char *const vmode_names[] = { "640x480", "640x240", "320x240", "320x200", "160x200" };
static const char *const scan_names[]  = { "off", "light", "medium", "heavy" };
static const char *const smooth_names[]= { "sharp", "soft", "sharp-fit" };
static const char *const cpu_names[]   = { "202.5 MHz", "162 MHz", "121.5 MHz", "81 MHz", "60 MHz",
                                           "40.5 MHz", "30 MHz", "20 MHz", "15 MHz", "10 MHz" };
static const char *const chord_names[] = { "Super+PageUp", "Ctrl+PageUp", "Alt+PageUp", "Ctrl+Alt+Del" };
static const char *const mkey_names[]  = { "F7", "F8", "F11", "Pause" };

static const set_desc desc[SET_COUNT] = {
    { "video.border",        "Border width",   ST_INT,   0, 0, 64, 4, 0, 0, SF_LIVE },
    { "video.border_colour", "Border colour",  ST_INT,   6, 0, 15, 1, 0, 0, SF_LIVE },
    { "video.font",          "Screen font",    ST_ENUM,  FONT_KERNEL8, 0, 0, 0, font_names, FONT_COUNT, SF_LIVE },
    { "video.mode",          "Resolution",     ST_ENUM,  VMODE_640x240, 0, 0, 0, vmode_names, VMODE_COUNT, SF_LIVE },
    { "video.margin",        "Left/top margin",ST_BOOL,  0, 0, 1, 1, 0, 0, SF_LIVE },   /* off: the full 80x30; use the border instead */
    { "video.scanlines",     "Scanlines",      ST_ENUM,  SCAN_OFF, 0, 0, 0, scan_names, SCAN_COUNT, SF_LIVE },
    { "video.smoothing",     "Scaling",        ST_ENUM,  SMOOTH_SHARP, 0, 0, 0, smooth_names, SMOOTH_COUNT, SF_LIVE },
    { "video.fullscreen",    "Full screen",    ST_BOOL,  0, 0, 1, 1, 0, 0, SF_LIVE },
    { "audio.volume",        "Volume",         ST_INT,   80, 0, 100, 10, 0, 0, SF_LIVE },
    { "input.reset_chord",   "Reset chord",    ST_CHORD, CHORD_SUPER_PGUP, 0, 0, 0, chord_names, CHORD_COUNT, SF_LIVE },
    { "input.menu_key",      "Menu key",       ST_ENUM,  MENUKEY_F7, 0, 0, 0, mkey_names, MENUKEY_COUNT, SF_LIVE },
    { "shell.cpm_com",       "CP/M .COM by name", ST_BOOL, 0, 0, 1, 1, 0, 0, SF_LIVE },   /* off: typing d must not launch a Z80 program */
    { "shell.startup",       "Run STARTUP.BAT", ST_BOOL, 1, 0, 1, 1, 0, 0, SF_RESTART },  /* read at power-on: the way out of a bad one */
    /* The machine is a fantasy and its timings are suggestions.  A Pi 3B+
     * emulates about half of 40.5 MHz in real time; asked for all of it, it
     * runs the whole machine at 20 fps and the sound starves.  So the Pi
     * defaults to 15 MHz and runs in real time, and the desktop starts at
     * 40.5.  Neither is a ceiling: the menu offers everything the enum has,
     * because what a host can hold is a question about that host, not about
     * this machine.  A desktop measured over 120 MHz should be allowed to
     * run there; a Pi asked for 202.5 will crawl, and the setting is live,
     * so stepping back down is how you find out.  INFO reports whichever is
     * set. */
#ifdef K4510_PI
    { "cpu.clock",           "CPU clock",      ST_ENUM,  CPUCLK_15,   0, 0, 0, cpu_names, CPUCLK_COUNT, SF_LIVE },
#else
    { "cpu.clock",           "CPU clock",      ST_ENUM,  CPUCLK_40_5, 0, 0, 0, cpu_names, CPUCLK_COUNT, SF_LIVE },
#endif
    /* ...and those defaults are only where a host starts before it has been
     * measured.  With cpu.auto on, the frontend runs core/calib.c at power-on
     * and sets cpu.clock to the highest step the host holds with margin; the
     * result is kept here with the host it was taken on, so it is paid once.
     * Choosing a clock in the menu turns auto off: an explicit setting wins. */
    { "cpu.auto",            "Auto clock",     ST_BOOL,  1, 0, 1, 1, 0, 0, SF_RESTART },
    { "cpu.measured",        "Measured clock", ST_ENUM,  CPUCLK_15, 0, 0, 0, cpu_names, CPUCLK_COUNT, 0 },
    { "cpu.host",            "Measured on",    ST_INT,   0, 0, 0x7FFFFFFF, 1, 0, 0, 0 },
};
static const unsigned cpu_hz_table[CPUCLK_COUNT] = { 202500000u, 162000000u, 121500000u, 81000000u, 60000000u,
                                                     40500000u, 30000000u, 20000000u, 15000000u, 10000000u };
unsigned settings_cpu_hz_of(int step) { if (step < 0 || step >= CPUCLK_COUNT) step = 0; return cpu_hz_table[step]; }
unsigned settings_cpu_hz(void) { return settings_cpu_hz_of(settings_get(SET_CPU_CLOCK)); }
static int value[SET_COUNT];
static int changed;

const set_desc *settings_desc(set_id id) { return &desc[id]; }
/* An ENUM may have choices the menu does not offer: settings_set still accepts
 * them (the machine can be in one, and the row must say so) but stepping and
 * the popup stop short. */
int settings_choices(set_id id)
{
    if (id == SET_VIDEO_MODE) return VMODE_MENU_MAX + 1;
    return desc[id].nlabels;
}
int settings_get(set_id id) { return value[id]; }
static int clampv(set_id id, int v)
{
    const set_desc *d = &desc[id];
    if (d->type == ST_ENUM || d->type == ST_CHORD) { if (v < 0) v = 0; if (v >= d->nlabels) v = d->nlabels - 1; return v; }
    if (v < d->min) v = d->min; if (v > d->max) v = d->max;
    return v;
}
void settings_set(set_id id, int v) { v = clampv(id, v); if (value[id] != v) { value[id] = v; changed = 1; } }
void settings_label(set_id id, int idx, const char *text)
{
    const set_desc *d = &desc[id];
    if (d->labels == font_names && idx >= 0 && idx < d->nlabels) font_names[idx] = text;
}
void settings_step(set_id id, int dir)
{
    const set_desc *d = &desc[id]; int v = value[id];
    if (d->type == ST_ENUM || d->type == ST_CHORD) { int n = settings_choices(id); v += dir; if (v < 0) v = n - 1; if (v >= n) v = 0; }
    else if (d->type == ST_BOOL) v = !v;
    else v += dir * d->step;
    settings_set(id, v);
}
const char *settings_text(set_id id, char *buf, int max)
{
    const set_desc *d = &desc[id]; int v = value[id];
    if (d->type == ST_ENUM || d->type == ST_CHORD) return d->labels[clampv(id, v)];
    if (d->type == ST_BOOL) return v ? "on" : "off";
    if (id == SET_AUDIO_VOLUME) snprintf(buf, (size_t) max, "%d%%", v);
    else if (id == SET_VIDEO_BORDER) snprintf(buf, (size_t) max, "%d px", v);
    else snprintf(buf, (size_t) max, "%d", v);
    return buf;
}
static const char *file_text(set_id id, char *buf, int max)   /* what goes in the file: raw numbers, enum names */
{
    const set_desc *d = &desc[id];
    /* 320x200 and 160x200 are live only.  A machine that came back from a
     * power cycle in 160x200 is a place you cannot easily steer out of, so
     * what reaches the file is never below 320x240. */
    if (id == SET_VIDEO_MODE && value[id] > VMODE_SAVE_MAX)
        { snprintf(buf, (size_t) max, "%s", vmode_names[VMODE_SAVE_MAX]); return buf; }
    if (d->type == ST_ENUM || d->type == ST_CHORD || d->type == ST_BOOL) return settings_text(id, buf, max);
    snprintf(buf, (size_t) max, "%d", value[id]); return buf;
}
void settings_defaults(void) { for (int i = 0; i < SET_COUNT; i++) value[i] = desc[i].def; changed = 0; }
int settings_changed(void) { return changed; }

static int find_key(const char *k) { for (int i = 0; i < SET_COUNT; i++) if (!strcmp(desc[i].key, k)) return i; return -1; }
static int parse_value(set_id id, const char *v)
{
    const set_desc *d = &desc[id];
    if (d->type == ST_ENUM || d->type == ST_CHORD) { for (int i = 0; i < d->nlabels; i++) if (!strcasecmp(d->labels[i], v)) return i; return clampv(id, atoi(v)); }
    if (d->type == ST_BOOL) return (!strcasecmp(v, "on") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") || atoi(v)) ? 1 : 0;
    return clampv(id, atoi(v));
}
static void trim(char *s) { size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0; }
static int split(char *line, char **k, char **v)         /* "key = value" -> 1; comments and blanks -> 0 */
{
    char *p = line; while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#' || *p == ';') return 0;
    char *eq = strchr(p, '='); if (!eq) return 0;
    *eq = 0; *k = p; trim(p);
    p = eq + 1; while (*p == ' ' || *p == '\t') p++; *v = p; trim(p);
    return 1;
}
int settings_load(const char *path)
{
    FILE *f = fopen(path, "r"); char line[256];
    settings_defaults();
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *k, *v; char copy[256]; strcpy(copy, line);
        if (!split(copy, &k, &v)) continue;
        int id = find_key(k);
        if (id >= 0) value[id] = parse_value((set_id) id, v);
    }
    /* and again on the way in, in case the file was edited by hand */
    if (value[SET_VIDEO_MODE] > VMODE_SAVE_MAX) value[SET_VIDEO_MODE] = VMODE_SAVE_MAX;
    fclose(f); changed = 0;
    return 0;
}
int settings_save(const char *path)
{
    char *old[128]; int nold = 0, seen[SET_COUNT] = { 0 };
    FILE *f = fopen(path, "r"); char line[256];
    if (f) { while (nold < 128 && fgets(line, sizeof line, f)) { old[nold] = malloc(strlen(line) + 1); strcpy(old[nold++], line); } fclose(f); }
    f = fopen(path, "w"); if (!f) { for (int i = 0; i < nold; i++) free(old[i]); return -1; }
    if (!nold) fprintf(f, "# BMC-K4510 settings -- written by the F7 menu; edit freely, unknown keys are kept\nversion = 1\n");
    for (int i = 0; i < nold; i++) {                      /* the old lines, known keys rewritten in place */
        char *k, *v; char copy[256]; strcpy(copy, old[i]);
        int id = split(copy, &k, &v) ? find_key(k) : -1;
        if (id >= 0 && !seen[id]) { char b[32]; fprintf(f, "%s = %s\n", desc[id].key, file_text((set_id) id, b, sizeof b)); seen[id] = 1; }
        else if (id < 0) fputs(old[i], f);
        free(old[i]);
    }
    for (int i = 0; i < SET_COUNT; i++) if (!seen[i]) { char b[32]; fprintf(f, "%s = %s\n", desc[i].key, file_text((set_id) i, b, sizeof b)); }
    fclose(f); changed = 0;
    return 0;
}
