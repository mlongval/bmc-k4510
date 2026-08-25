/* The F7 menu and the settings registry: keys in, overlay and k4510.cfg out. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/ui/settings.h"
#include "../core/ui/menu.h"
#include "../core/ui/ui_draw.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static uint8_t ov[UI_W * UI_H], font[2048];
static int cell_is(int cx, int cy, int colour) { int n = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (ov[(cy * 8 + y) * UI_W + cx * 8 + x] == colour) n++; return n; }
int main(void)
{
    const char *cfg = "test/uitest.cfg"; FILE *f;
    mem_init(); io_reset();
    for (int i = 0; i < 2048; i++) font[i] = (uint8_t)(i * 37);     /* any font: the test only looks at colours */
    ui_font(font);
    /* 1. the registry and its file */
    f = fopen(cfg, "w"); fputs("# my notes\nvideo.border = 12\naudio.volume=30\nfuture.thing = keep me\nvideo.font = unscii\n", f); fclose(f);
    CHECK(settings_load(cfg) == 0, "load");
    CHECK(settings_get(SET_VIDEO_BORDER) == 12 && settings_get(SET_AUDIO_VOLUME) == 30 && settings_get(SET_VIDEO_FONT) == FONT_UNSCII, "values read (%d %d %d)", settings_get(SET_VIDEO_BORDER), settings_get(SET_AUDIO_VOLUME), settings_get(SET_VIDEO_FONT));
    CHECK(settings_get(SET_INPUT_MENU_KEY) == MENUKEY_F7 && !settings_changed(), "defaults for the rest, not dirty");
    settings_set(SET_VIDEO_BORDER, 999); CHECK(settings_get(SET_VIDEO_BORDER) == 64 && settings_changed(), "clamped, dirty");
    settings_step(SET_INPUT_RESET_CHORD, -1); CHECK(settings_get(SET_INPUT_RESET_CHORD) == CHORD_COUNT - 1, "enum wraps");
    CHECK(settings_save(cfg) == 0, "save");
    { char buf[1024] = { 0 }; f = fopen(cfg, "r"); fread(buf, 1, sizeof buf - 1, f); fclose(f);
      CHECK(strstr(buf, "# my notes") && strstr(buf, "future.thing = keep me"), "comments and unknown keys kept");
      CHECK(strstr(buf, "video.border = 64 px") == 0 && strstr(buf, "video.border = 64"), "border rewritten in place");
      CHECK(strstr(buf, "input.reset_chord = Ctrl+Alt+Del") && strstr(buf, "input.menu_key = F7"), "missing keys appended: '%s'", buf); }
    printf("1. registry: load, clamp, wrap, save with unknown keys kept\n");
    /* 2. keys reach the menu through kbd_push */
    settings_defaults();
    kbd_push('a'); CHECK(io_read(IO_KBD) == 'a' && !menu_is_open(), "a plain key reaches the machine");
    kbd_modifiers(1, 0, 0); kbd_push(KEY_F1 + 6); CHECK(!menu_is_open() && io_read(IO_KBD) == KEY_F1 + 6, "Shift+F7 reaches the machine");
    kbd_modifiers(0, 0, 0); kbd_push(KEY_F1 + 6); CHECK(menu_is_open(), "F7 opens the menu");
    kbd_push('x'); CHECK(io_read(IO_KBD) == 0, "keys do not reach the machine while open");
    CHECK(menu_draw(ov) == 1 && menu_draw(ov) == 0, "draws once, then clean");
    { int x0 = (UI_COLS - 44) / 2, y0 = (UI_ROWS - 15) / 2;
      CHECK(cell_is(x0, y0, UIC_FRAME) > 0 && cell_is(x0 + 2, y0 + 2, UIC_BAR) > 0, "frame and cursor bar drawn");
      CHECK(ov[0] == 0, "outside the window: see-through"); }
    kbd_push(KEY_DOWN); kbd_push(KEY_ENTER);          /* Audio */
    kbd_push(KEY_RIGHT); CHECK(settings_get(SET_AUDIO_VOLUME) == 90, "Right steps the volume (%d)", settings_get(SET_AUDIO_VOLUME));
    kbd_push(KEY_LEFT); kbd_push(KEY_LEFT); CHECK(settings_get(SET_AUDIO_VOLUME) == 70, "Left steps back");
    kbd_push(KEY_ESC); kbd_push(KEY_UP); kbd_push(KEY_ENTER);   /* Video */
    kbd_push(KEY_DOWN); kbd_push(KEY_DOWN); kbd_push(KEY_ENTER);  /* Screen font: a popup */
    kbd_push(KEY_DOWN); kbd_push(KEY_DOWN); kbd_push(KEY_ENTER);
    CHECK(settings_get(SET_VIDEO_FONT) == FONT_OPENROMS, "popup chose open-roms (%d)", settings_get(SET_VIDEO_FONT));
    kbd_push(KEY_ESC); kbd_push(KEY_DOWN); kbd_push(KEY_DOWN); kbd_push(KEY_DOWN); kbd_push(KEY_ENTER);   /* Machine */
    kbd_push(KEY_DOWN); kbd_push(KEY_DOWN); kbd_push(KEY_ENTER);   /* past Save/Load state (the separator is skipped): Reset */
    CHECK(!menu_is_open() && menu_take_action() == ACT_RESET && menu_take_action() == ACT_NONE, "Reset acts and closes");
    CHECK(menu_closed_pending() == 1 && menu_closed_pending() == 0, "close reported once");
    kbd_push('b'); CHECK(io_read(IO_KBD) == 'b', "keys reach the machine again");
    settings_set(SET_INPUT_MENU_KEY, MENUKEY_F8); kbd_push(KEY_F1 + 6); CHECK(!menu_is_open(), "F7 is a plain key once the menu key moved");
    kbd_push(KEY_F1 + 7); CHECK(menu_is_open(), "F8 opens it"); menu_close();
    printf("2. menu: open/close, navigation, INT steps, ENUM popup, actions\n");
    remove(cfg);
    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails); return fails != 0;
}
