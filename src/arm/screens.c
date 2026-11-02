/*
 * Title and stage-select screens. All art here is original. Cell columns 0-1 are
 * hidden by the HMOVE bar, so everything sits in columns 2-39.
 *
 * Statics are assigned in the *_enter() functions: RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "display_list.h"
#include "fb.h"
#include "screens.h"

#define VIS_COL   2                 /* first visible cell column */
#define VIS_W     38                /* visible cell columns */

#define COLOR_PF  0x1Eu             /* gold */

/* Column that centres a string of the given pixel scale in the visible area. */
static int centered(const char *s, int sx)
{
    return VIS_COL + ((VIS_W - fb_text_width(s, sx)) >> 1);
}

/* ---- stage list ---- */

static const char *const names[STAGE_COUNT] = {
    "GROVE", "TIDE", "EMBER", "FROST", "GALE", "QUARRY", "VOLT", "GEAR"
};

const char *stage_name(int stage)
{
    return stage >= 0 && stage < STAGE_COUNT ? names[stage] : "";
}

int stage_playable(int stage)
{
    return stage == 0;
}

int stage_at(int gx, int gy)
{
    const int g = gy * 3 + gx;

    if (g == 4)
        return -1;
    return g > 4 ? g - 1 : g;
}

/* ---- title ---- */

#define TITLE_FLOOR 168             /* first line of the floor */
#define HERO_RUN_Y  (TITLE_FLOOR - 16)

void screen_title_enter(void)
{
    int x;

    fb_clear();
    fb_fill(4, 10, VIS_W - 2, 2, 1);
    fb_text(centered("MEH-GA", 1), 18, "MEH-GA", 1, 5);
    fb_text(centered("MAN", 2), 48, "MAN", 2, 6);
    fb_fill(4, 84, VIS_W - 2, 2, 1);

    /* Floor: solid, with a groove and brick joints. */
    fb_fill(VIS_COL, TITLE_FLOOR, VIS_W, 24, 1);
    fb_fill(VIS_COL, TITLE_FLOOR + 6, VIS_W, 1, 0);
    fb_fill(VIS_COL, TITLE_FLOOR + 15, VIS_W, 1, 0);
    for (x = VIS_COL + 4; x < FB_COLS; x += 8) {
        fb_fill(x, TITLE_FLOOR + 7, 1, 8, 0);
        fb_fill(x + 4, TITLE_FLOOR + 16, 1, 8, 0);
    }
}

void screen_title_frame(unsigned tick)
{
    /* Blinking prompt: on for 32 frames, off for 32. */
    fb_fill(VIS_COL, 110, VIS_W, 15, 0);
    if (!(tick & 32u))
        fb_text(centered("START", 1), 110, "START", 1, 3);

    fb_present();
    display_sky(0x80u, 1);
    display_one_sprite(8 + (int)((tick >> 1) & 127u), HERO_RUN_Y, ART_HERO);
    display_finish(COLOR_PF);
}

/* ---- stage select ---- */

#define SLOT_COL(gx)  (3 + (gx) * 12)       /* a slot is 12 cells x 40 lines */
#define SLOT_LINE(gy) (30 + (gy) * 44)
#define SLOT_W        12
#define SLOT_H        40
#define NAME_LINE     166

/* 8x8 icons, bit 7 leftmost, one per grid cell; each row is drawn 4 lines tall. */
static const uint8_t icons[9][8] = {
    {0x18, 0x3C, 0x7E, 0xFF, 0x7E, 0x18, 0x18, 0x18},   /* grove: a tree */
    {0x00, 0x00, 0x66, 0x99, 0x00, 0x66, 0x99, 0x00},   /* tide: waves */
    {0x10, 0x18, 0x38, 0x7C, 0x7E, 0xFF, 0xFF, 0x7E},   /* ember: a flame */
    {0x18, 0x99, 0x5A, 0x3C, 0x3C, 0x5A, 0x99, 0x18},   /* frost: a flake */
    {0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x7E, 0x3C, 0x24},   /* centre: the hero */
    {0x3E, 0x41, 0x01, 0x1E, 0x00, 0x78, 0x80, 0x7E},   /* gale: wind */
    {0x38, 0x7C, 0xFE, 0xDE, 0xFE, 0xFE, 0x7C, 0x38},   /* quarry: a boulder */
    {0x0E, 0x1C, 0x38, 0x7E, 0x1C, 0x38, 0x70, 0x20},   /* volt: a bolt */
    {0x5A, 0xFF, 0x66, 0xE7, 0xE7, 0x66, 0xFF, 0x5A},   /* gear */
};

static int8_t prev_gx;
static int8_t prev_gy;

static void draw_slot(int gx, int gy)
{
    const int g = gy * 3 + gx;
    const int bx = SLOT_COL(gx) + 1;
    const int by = SLOT_LINE(gy) + 2;
    const int stage = stage_at(gx, gy);
    const int locked = stage >= 0 && !stage_playable(stage);
    int r;

    fb_fill(bx, by, 10, 36, 1);             /* frame */
    fb_fill(bx + 1, by + 2, 8, 32, 0);      /* inside */
    for (r = 0; r < 8; r++) {
        unsigned row = icons[g][r];

        if (locked)                         /* dithered until the stage exists */
            row &= (r & 1) ? 0xAAu : 0x55u;
        fb_row8(bx + 1, by + 2 + r * 4, 4, row);
    }
}

/* The cursor: an outline in the margin of a slot. */
static void draw_cursor(int gx, int gy, int on)
{
    const int c = SLOT_COL(gx);
    const int l = SLOT_LINE(gy);

    fb_fill(c, l, SLOT_W, 2, on);
    fb_fill(c, l + SLOT_H - 2, SLOT_W, 2, on);
    fb_fill(c, l, 1, SLOT_H, on);
    fb_fill(c + SLOT_W - 1, l, 1, SLOT_H, on);
}

void screen_select_enter(int gx, int gy)
{
    int x, y;

    fb_clear();
    fb_text(centered("SELECT", 1), 4, "SELECT", 1, 4);
    fb_fill(4, 26, VIS_W - 2, 2, 1);
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++)
            draw_slot(x, y);
    prev_gx = (int8_t)gx;
    prev_gy = (int8_t)gy;
}

void screen_select_frame(int gx, int gy, unsigned tick)
{
    const int stage = stage_at(gx, gy);
    const char *name = stage >= 0 ? stage_name(stage) : "MEH-GA";

    if (gx != prev_gx || gy != prev_gy) {
        draw_cursor(prev_gx, prev_gy, 0);
        prev_gx = (int8_t)gx;
        prev_gy = (int8_t)gy;
    }
    draw_cursor(gx, gy, !(tick & 8u));      /* blinks: 8 frames on, 8 off */

    fb_fill(VIS_COL, NAME_LINE, VIS_W, 22, 0);
    fb_text(centered(name, 1), NAME_LINE, name, 1, 4);

    fb_present();
    display_sky(0x80u, 1);
    display_no_sprites();
    display_finish(COLOR_PF);
}
