/* The F7 menu. See menu.h. */
#include "menu.h"
#include "settings.h"
#include "ui_draw.h"
#include "../io.h"
#include <string.h>
#include <stdio.h>

typedef enum { MI_SUBMENU, MI_SETTING, MI_ACTION, MI_INFO, MI_SEP, MI_SAVESLOT, MI_LOADSLOT } item_kind;
typedef struct menu_s menu_t;
typedef struct { const char *label; item_kind kind; int arg; const menu_t *sub; } item_t;
struct menu_s { const char *title; const item_t *items; int n; };

static const item_t video_items[] = {
    { "Border width",  MI_SETTING, SET_VIDEO_BORDER },
    { "Border colour", MI_SETTING, SET_VIDEO_BORDER_COLOUR },
    { "Screen font",   MI_SETTING, SET_VIDEO_FONT },
    { "Full screen",   MI_SETTING, SET_VIDEO_FULLSCREEN },
};
static const item_t audio_items[] = { { "Volume", MI_SETTING, SET_AUDIO_VOLUME } };
static const item_t input_items[] = {
    { "Reset chord", MI_SETTING, SET_INPUT_RESET_CHORD },
    { "Menu key",    MI_SETTING, SET_INPUT_MENU_KEY },
};
static const item_t save_items[] = {
    { "Slot 1", MI_SAVESLOT, 0 }, { "Slot 2", MI_SAVESLOT, 1 }, { "Slot 3", MI_SAVESLOT, 2 }, { "Slot 4", MI_SAVESLOT, 3 },
};
static const item_t load_items[] = {
    { "Slot 1", MI_LOADSLOT, 0 }, { "Slot 2", MI_LOADSLOT, 1 }, { "Slot 3", MI_LOADSLOT, 2 }, { "Slot 4", MI_LOADSLOT, 3 },
};
static const menu_t save_menu = { "Save state", save_items, 4 };
static const menu_t load_menu = { "Load state", load_items, 4 };
static const item_t machine_items[] = {
    { "Save state",        MI_SUBMENU, 0, &save_menu },
    { "Load state",        MI_SUBMENU, 0, &load_menu },
    { "",                  MI_SEP },
    { "Reset",             MI_ACTION, ACT_RESET },
    { "Power cycle",       MI_ACTION, ACT_POWER_CYCLE },
    { "Stop the Tube",     MI_ACTION, ACT_TUBE_STOP },
    { "",                  MI_SEP },
    { "Quit the emulator", MI_ACTION, ACT_QUIT },
};
static const item_t info_items[] = {
    { "Version", MI_INFO, INFO_VERSION }, { "ROM", MI_INFO, INFO_ROM }, { "Files", MI_INFO, INFO_FS }, { "Host", MI_INFO, INFO_HOST },
};
static const menu_t video_menu   = { "Video",   video_items,   4 };
static const menu_t audio_menu   = { "Audio",   audio_items,   1 };
static const menu_t input_menu   = { "Input",   input_items,   2 };
static const menu_t machine_menu = { "Machine", machine_items, 8 };
static const menu_t info_menu    = { "Info",    info_items,    4 };
static const item_t main_items[] = {
    { "Video",   MI_SUBMENU, 0, &video_menu },
    { "Audio",   MI_SUBMENU, 0, &audio_menu },
    { "Input",   MI_SUBMENU, 0, &input_menu },
    { "Machine", MI_SUBMENU, 0, &machine_menu },
    { "Info",    MI_SUBMENU, 0, &info_menu },
};
static const menu_t main_menu = { "BMC-K4510", main_items, 5 };

/* ---- state ---------------------------------------------------------------- */
static struct { const menu_t *m; int cur; } stack[6];
static int depth, open_, dirty, action, closed;
static int popup, popup_cur;                 /* an ENUM's option list, over the window */
static char info[INFO_COUNT][40];
static char slot[MENU_SLOTS][24];

static const menu_t *top(void) { return stack[depth].m; }
static void move_cur(int d)
{
    const menu_t *m = top(); int c = stack[depth].cur;
    for (int i = 0; i < m->n; i++) { c = (c + d + m->n) % m->n; if (m->items[c].kind != MI_SEP) break; }
    stack[depth].cur = c;
}
void menu_open(void) { open_ = 1; depth = 0; stack[0].m = &main_menu; stack[0].cur = 0; popup = 0; dirty = 1; }
void menu_close(void) { if (open_) { open_ = 0; closed = 1; dirty = 1; } }
int  menu_is_open(void) { return open_; }
int  menu_take_action(void) { int a = action; action = ACT_NONE; return a; }
int  menu_closed_pending(void) { int c = closed; closed = 0; return c; }
void menu_info(int row, const char *text) { if (row >= 0 && row < INFO_COUNT) { snprintf(info[row], sizeof info[row], "%s", text); dirty = 1; } }
void menu_slot(int n, const char *text) { if (n >= 0 && n < MENU_SLOTS) { snprintf(slot[n], sizeof slot[n], "%s", text); dirty = 1; } }
int  menu_key_code(void)
{
    static const uint8_t codes[MENUKEY_COUNT] = { KEY_F1 + 6, KEY_F1 + 7, KEY_F1 + 10, 0x9F };
    return codes[settings_get(SET_INPUT_MENU_KEY)];
}

static void enter(void)
{
    const item_t *it = &top()->items[stack[depth].cur];
    switch (it->kind) {
    case MI_SUBMENU: if (depth < 5) { depth++; stack[depth].m = it->sub; stack[depth].cur = 0; } break;
    case MI_ACTION: action = it->arg; if (it->arg != ACT_TUBE_STOP) menu_close(); break;
    case MI_SAVESLOT: action = ACT_SAVE_SLOT + it->arg; break;               /* the menu stays: the host refreshes the slot's text */
    case MI_LOADSLOT: action = ACT_LOAD_SLOT + it->arg; menu_close(); break;
    case MI_SETTING: {
        const set_desc *d = settings_desc((set_id) it->arg);
        if (d->type == ST_ENUM || d->type == ST_CHORD) { popup = 1; popup_cur = settings_get((set_id) it->arg); }
        else settings_step((set_id) it->arg, +1);
        break; }
    default: break;
    }
}
void menu_key(uint8_t k)
{
    if (!open_) return;
    dirty = 1;
    if (popup) {
        const item_t *it = &top()->items[stack[depth].cur]; const set_desc *d = settings_desc((set_id) it->arg);
        if (k == KEY_UP) popup_cur = (popup_cur + d->nlabels - 1) % d->nlabels;
        else if (k == KEY_DOWN) popup_cur = (popup_cur + 1) % d->nlabels;
        else if (k == KEY_ENTER) { settings_set((set_id) it->arg, popup_cur); popup = 0; }
        else if (k == KEY_ESC) popup = 0;
        return;
    }
    if (k == menu_key_code() || (k == KEY_ESC && depth == 0)) { menu_close(); return; }
    if (k == KEY_ESC || k == KEY_BS) { depth--; return; }
    if (k == KEY_UP) move_cur(-1);
    else if (k == KEY_DOWN) move_cur(+1);
    else if (k == KEY_ENTER || k == KEY_RIGHT || k == ' ') {
        const item_t *it = &top()->items[stack[depth].cur];
        if (k == KEY_RIGHT && it->kind == MI_SETTING) settings_step((set_id) it->arg, +1); else enter();
    }
    else if (k == KEY_LEFT) {
        const item_t *it = &top()->items[stack[depth].cur];
        if (it->kind == MI_SETTING) settings_step((set_id) it->arg, -1); else if (depth) depth--;
    }
    else if (k == KEY_HOME) stack[depth].cur = 0;
    else if (k == KEY_END) { stack[depth].cur = top()->n - 1; if (top()->items[stack[depth].cur].kind == MI_SEP) move_cur(-1); }
}

/* ---- drawing ---------------------------------------------------------------- */
#define WIN_W 44
#define WIN_H 15
int menu_draw(uint8_t *ov)
{
    if (!dirty) return 0;
    dirty = 0;
    ui_clear(ov);
    if (!open_) return 1;
    const menu_t *m = top();
    int x0 = (UI_COLS - WIN_W) / 2, y0 = (UI_ROWS - WIN_H) / 2;
    char title[64], b[32];
    ui_box(ov, x0, y0, WIN_W, WIN_H, UIC_FRAME, UIC_PANEL);
    if (depth) snprintf(title, sizeof title, " %s: %s ", main_menu.title, m->title); else snprintf(title, sizeof title, " %s ", m->title);
    ui_text(ov, x0 + (WIN_W - (int) strlen(title)) / 2, y0, UIC_TITLE, UIC_PANEL, title);
    for (int i = 0; i < m->n; i++) {
        const item_t *it = &m->items[i]; int y = y0 + 2 + i;
        int sel = (i == stack[depth].cur);
        uint8_t fg = sel ? UIC_BARTEXT : UIC_TEXT, bg = sel ? UIC_BAR : UIC_PANEL;
        if (it->kind == MI_SEP) continue;
        ui_fill(ov, x0 + 2, y, WIN_W - 4, 1, bg);
        ui_text(ov, x0 + 3, y, fg, bg, it->label);
        const char *v = 0;
        if (it->kind == MI_SETTING) v = settings_text((set_id) it->arg, b, sizeof b);
        else if (it->kind == MI_INFO) v = info[it->arg];
        else if (it->kind == MI_SAVESLOT || it->kind == MI_LOADSLOT) v = slot[it->arg][0] ? slot[it->arg] : "empty";
        else if (it->kind == MI_SUBMENU) v = ">";
        if (v) ui_text(ov, x0 + WIN_W - 3 - (int) strlen(v), y, it->kind == MI_INFO && !sel ? UIC_DIM : fg, bg, v);
    }
    const char *legend = depth ? " Esc Back   Enter Select   F7 Close " : " Enter Select   F7/Esc Close ";
    if (depth == 0 && settings_get(SET_INPUT_MENU_KEY) != MENUKEY_F7) legend = " Enter Select   Esc Close ";
    ui_text(ov, x0 + (WIN_W - (int) strlen(legend)) / 2, y0 + WIN_H - 1, UIC_DIM, UIC_PANEL, legend);
    if (popup) {
        const item_t *it = &m->items[stack[depth].cur]; const set_desc *d = settings_desc((set_id) it->arg);
        int w = 8; for (int i = 0; i < d->nlabels; i++) if ((int) strlen(d->labels[i]) + 6 > w) w = (int) strlen(d->labels[i]) + 6;
        int h = d->nlabels + 2, px = (UI_COLS - w) / 2, py = (UI_ROWS - h) / 2;
        ui_box(ov, px, py, w, h, UIC_FRAME, UIC_PANEL);
        snprintf(title, sizeof title, " %s ", it->label);
        ui_text(ov, px + (w - (int) strlen(title)) / 2, py, UIC_TITLE, UIC_PANEL, title);
        for (int i = 0; i < d->nlabels; i++) {
            int sel = (i == popup_cur); uint8_t fg = sel ? UIC_BARTEXT : UIC_TEXT, bg = sel ? UIC_BAR : UIC_PANEL;
            ui_fill(ov, px + 1, py + 1 + i, w - 2, 1, bg);
            ui_text(ov, px + 2, py + 1 + i, fg, bg, i == settings_get((set_id) it->arg) ? "\xFB" : " ");
            ui_text(ov, px + 4, py + 1 + i, fg, bg, d->labels[i]);
        }
    }
    return 1;
}
