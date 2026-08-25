/* The settings registry. See settings.h. */
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *font_names[]  = { "kernel8", "unscii", "open-roms", "PXLfont", "C64 chargen" };   /* the last one is renamed by the host if /SYSTEM/chargen.bin is absent */
static const char *const chord_names[] = { "Super+PageUp", "Ctrl+PageUp", "Alt+PageUp", "Ctrl+Alt+Del" };
static const char *const mkey_names[]  = { "F7", "F8", "F11", "Pause" };

static const set_desc desc[SET_COUNT] = {
    { "video.border",        "Border width",   ST_INT,   0, 0, 64, 4, 0, 0, SF_LIVE },
    { "video.border_colour", "Border colour",  ST_INT,   6, 0, 15, 1, 0, 0, SF_LIVE },
    { "video.font",          "Screen font",    ST_ENUM,  FONT_KERNEL8, 0, 0, 0, font_names, FONT_COUNT, SF_LIVE },
    { "video.fullscreen",    "Full screen",    ST_BOOL,  0, 0, 1, 1, 0, 0, SF_LIVE },
    { "audio.volume",        "Volume",         ST_INT,   80, 0, 100, 10, 0, 0, SF_LIVE },
    { "input.reset_chord",   "Reset chord",    ST_CHORD, CHORD_SUPER_PGUP, 0, 0, 0, chord_names, CHORD_COUNT, SF_LIVE },
    { "input.menu_key",      "Menu key",       ST_ENUM,  MENUKEY_F7, 0, 0, 0, mkey_names, MENUKEY_COUNT, SF_LIVE },
};
static int value[SET_COUNT];
static int changed;

const set_desc *settings_desc(set_id id) { return &desc[id]; }
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
    if (d->type == ST_ENUM || d->type == ST_CHORD) { v += dir; if (v < 0) v = d->nlabels - 1; if (v >= d->nlabels) v = 0; }
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
