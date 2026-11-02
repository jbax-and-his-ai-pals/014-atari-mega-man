/*
 * The stage-select screen, in scene mode: a striped blue background, a 3x3 grid of
 * framed emblems with the hero in the middle, a short name under each, and the
 * highlighted stage's full name at the bottom. All art here is original. The scene
 * kernel has no HMOVE bar, so all 40 cell columns are visible.
 *
 * The playfield and the per-line colors are drawn once in screen_select_enter();
 * only the blinking cursor and the footer change afterward.
 *
 * Statics are assigned in screen_select_enter(): RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "cdfj.h"
#include "display_list.h"
#include "fb.h"
#include "screens.h"

#define VIS_COL   0                 /* first visible cell column */
#define VIS_W     40                /* visible cell columns */

/* ---- the roster ---- */

static const char *const full_names[STAGE_COUNT] = {
    "SAW MAN", "SEA MAN", "ICE MAN", "SUN MAN", "OAK MAN", "ARC MAN", "COG MAN", "JET MAN"
};

static const char *const short_names[STAGE_COUNT] = {
    "SAW", "SEA", "ICE", "SUN", "OAK", "ARC", "COG", "JET"
};

const char *stage_name(int stage)
{
    return stage >= 0 && stage < STAGE_COUNT ? full_names[stage] : "";
}

const char *stage_short(int stage)
{
    return stage >= 0 && stage < STAGE_COUNT ? short_names[stage] : "";
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

/* ---- layout ---- */

#define SLOT_COL(gx)  (1 + (gx) * 13)       /* a slot is 12 cells x 40 lines, 13 apart */
#define SLOT_LINE(gy) (24 + (gy) * 52)
#define SLOT_W        12
#define SLOT_H        40
#define NAME_OFFSET   42                    /* short name, below the slot */
#define FOOTER_LINE   181

/* Emblems: 8 cells x 16 rows, bit 7 leftmost, one per grid cell; each row is 2 lines. */
static const uint8_t icons[9][16] = {
    {   /* saw: a toothed disc with a hub */
        0b00011000, 0b01011010, 0b00111100, 0b11111111, 0b01111110, 0b11111111, 0b11111110, 0b11100111,
        0b11100111, 0b01111111, 0b11111111, 0b01111110, 0b11111111, 0b00111100, 0b01011010, 0b00011000
    },
    {   /* sea: three rows of waves */
        0b00000000, 0b00000000, 0b01100110, 0b10011001, 0b00000000, 0b00000000, 0b00000000, 0b01100110,
        0b10011001, 0b00000000, 0b00000000, 0b00000000, 0b01100110, 0b10011001, 0b00000000, 0b00000000
    },
    {   /* ice: a faceted crystal */
        0b00011000, 0b00011000, 0b00111100, 0b00101100, 0b01111110, 0b01011010, 0b11111111, 0b10111101,
        0b10111101, 0b11111111, 0b01011010, 0b01111110, 0b00101100, 0b00111100, 0b00011000, 0b00011000
    },
    {   /* sun: a disc with rays */
        0b00011000, 0b10011001, 0b01011010, 0b00111100, 0b00111100, 0b11111111, 0b01111110, 0b11111111,
        0b11111111, 0b01111110, 0b11111111, 0b00111100, 0b00111100, 0b01011010, 0b10011001, 0b00011000
    },
    {   /* the hero: helmet, half-lidded eyes, shoulders */
        0b00111100, 0b01111110, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11011011, 0b11111111,
        0b01111110, 0b00111100, 0b00011000, 0b01111110, 0b11111111, 0b11111111, 0b11100111, 0b11000011
    },
    {   /* oak: a crown on a trunk */
        0b00111100, 0b01111110, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b01111110, 0b00111100,
        0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b01111110, 0b00000000
    },
    {   /* arc: a lightning bolt */
        0b00000000, 0b00001110, 0b00011100, 0b00111000, 0b01110000, 0b11111110, 0b01111110, 0b00011100,
        0b00111000, 0b01110000, 0b11100000, 0b11000000, 0b10000000, 0b00000000, 0b00000000, 0b00000000
    },
    {   /* cog: a gear with a big hole */
        0b00111100, 0b10111101, 0b11111111, 0b01111110, 0b11111111, 0b11100111, 0b11000011, 0b11000011,
        0b11000011, 0b11000011, 0b11100111, 0b11111111, 0b01111110, 0b11111111, 0b10111101, 0b00111100
    },
    {   /* jet: a plane from above */
        0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b00111100, 0b01111110, 0b11111111, 0b11111111,
        0b00111100, 0b00111100, 0b00011000, 0b00011000, 0b01111110, 0b01111110, 0b00011000, 0b00000000
    },
};

static int8_t prev_gx;
static int8_t prev_gy;

/* Background: deep blue with light stripes behind each row of frames. */
static uint8_t select_bk(int y)
{
    int r;

    if (y < 3 || y >= 190)
        return 0x9Au;
    for (r = 0; r < 3; r++)
        if (y >= SLOT_LINE(r) + 17 && y < SLOT_LINE(r) + 21)
            return 0x9Au;
    return 0x84u;
}

static uint8_t select_pf_color(int y)
{
    return y >= FOOTER_LINE ? 0x1Eu : 0x0Eu;        /* the footer is gold, the rest white */
}

static int slot_text_col(const char *s)
{
    return VIS_COL + ((VIS_W - fb_text_width(s, 1)) >> 1);
}

static void draw_slot(int gx, int gy)
{
    const int g = gy * 3 + gx;
    const int bx = SLOT_COL(gx) + 1;
    const int by = SLOT_LINE(gy) + 2;
    const int stage = stage_at(gx, gy);
    const char *label = stage >= 0 ? stage_short(stage) : "MEH";
    int r;

    fb_fill(bx, by, 10, 36, 1);             /* frame */
    fb_fill(bx + 1, by + 2, 8, 32, 0);      /* inside */
    for (r = 0; r < 16; r++) {
        fb_row8(bx + 1, by + 2 + r * 2, 2, icons[g][r]);
    }
    fb_text_ex(SLOT_COL(gx) + ((SLOT_W - fb_text_width_ex(label, 1, 0)) >> 1),
               SLOT_LINE(gy) + NAME_OFFSET, label, 1, 2, 0);
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
    fb_text(slot_text_col("SELECT"), 6, "SELECT", 1, 3);
    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++)
            draw_slot(x, y);
    prev_gx = (int8_t)gx;
    prev_gy = (int8_t)gy;

    for (y = 0; y <= LINES; y++) {
        display_bk_line(y, select_bk(y));
        display_pfcolor_line(y, select_pf_color(y));
    }
    display_no_sprites();
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

    fb_fill(VIS_COL, FOOTER_LINE, VIS_W, 10, 0);
    fb_text(slot_text_col(name), FOOTER_LINE, name, 1, 2);

    fb_present();
    display_finish_scene();
}
