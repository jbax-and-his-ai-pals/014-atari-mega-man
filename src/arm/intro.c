/*
 * The intro, in two parts. All the art is original and generated from a few
 * numbers, so it costs almost no ROM.
 *
 * Part 1, the pan: a tower of four setback tiers with a spire, two small side
 * buildings and a street, in "world lines" counted from the spire's tip. The
 * camera's scroll is the world line at the top of the screen; it starts near the
 * street and rises to the roof, where the hero stands. The tower is the playfield;
 * its windows are holes that show the sky. This part uses the game kernel.
 *
 * Part 2, the vista: a night sky with stars, a blue glow behind a jagged ridge of
 * mountains, a skyline of lit windows, and a rooftop at the right where the hero
 * stands, with the title above. This part uses the scene kernel, which streams a
 * playfield color per line, so the title can be banded, the mountains black, and
 * the city and rooftop light, all in one frame.
 *
 * Title text lives in the framebuffer and is XORed on top of the picture, so it
 * stays readable over anything.
 *
 * Statics are assigned in intro_enter(): RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "art_data.h"
#include "cdfj.h"
#include "display_list.h"
#include "fb.h"
#include "intro.h"

/* ---- part 1: the pan ---- */

#define WORLD_GROUND  470           /* world line where the street starts */
#define SCROLL_START  330           /* camera at the start: the street is in view */
#define SCROLL_END    (-96)         /* camera at the end: sky above the roof */
#define ROOF_Y        36            /* world line of the top tier's roof */
#define SPIRE_COL     20
#define NUM_TIERS     4
#define PAN_FAST      2             /* lines per frame */
#define PAN_SLOW      1
#define SLOW_ZONE     48            /* the last lines of the pan go slowly */
#define PAN_HOLD      40            /* frames on the roof before the cut to the vista */
#define PAN_PF        0x0Cu         /* the tower's color: light steel */

/* Each tier is a block of cells c0..c1 from world line y0 down to the next tier. */
static const struct { int16_t y0; uint8_t c0, c1; } tiers[NUM_TIERS] = {
    { ROOF_Y, 14, 25 }, { 96, 11, 28 }, { 176, 8, 31 }, { 296, 5, 34 }
};

/* ---- part 2: the vista (screen lines) ---- */

#define V_GLOW_TOP    92            /* the glow starts here, black above */
#define V_MT_TOP      100           /* highest mountain peak */
#define V_CITY_TOP    124           /* mountains end; the city and rooftop start */
#define V_ROOF        140           /* the roof surface */
#define V_HERO_X      112
#define V_TITLE_1     12            /* frames after the cut until MEH-GA appears */
#define V_TITLE_2     28
#define V_READY       50

#define VIS_COL 2                   /* first visible cell column (HMOVE bar hides 0-1) */
#define VIS_W   38

/* Top line of the ridge at each cell. */
static const uint8_t ridge[40] = {
    116, 114, 112, 110, 108, 106, 108, 111, 113, 112,
    109, 106, 103, 101, 103, 106, 109, 111, 112, 110,
    108, 106, 108, 111, 113, 114, 112, 109, 107, 105,
    107, 110, 113, 115, 114, 112, 110, 108, 110, 113
};

/* The skyline: lit windows only, so the buildings read as night shapes. */
static const struct { uint8_t c0, c1, top; } city[5] = {
    { 2, 5, 148 }, { 7, 10, 134 }, { 12, 14, 152 }, { 15, 18, 140 }, { 20, 22, 150 }
};

/* Title colors by font row: a cool gradient from light blue to white and back. */
static const uint8_t ramp[5] = { 0xAA, 0xAE, 0x0E, 0xAE, 0xA8 };

static uint8_t phase;               /* 0 = the pan, 1 = the vista */
static int16_t scroll;
static uint16_t hold;               /* frames since the pan reached the roof */
static uint16_t vtick;              /* frames since the cut to the vista */
static uint8_t drawn;               /* title text drawn: 0 none, 1 MEH-GA, 2 both */
static int8_t prompt_on;            /* whether the START prompt is drawn right now */
static uint32_t body_lo[NUM_TIERS]; /* each tier's cells: lo = cells 0-19, hi = 20-39 */
static uint32_t body_hi[NUM_TIERS];
static int16_t cache_row;           /* the window row whose open windows are cached */
static int8_t cache_tier;
static uint32_t cache_lo;
static uint32_t cache_hi;

static void vista_draw_all(unsigned tick);
static void vista_frame(unsigned tick);

static void add_cell(uint32_t *lo, uint32_t *hi, int col)
{
    if (col < 20)
        *lo |= 1u << col;
    else
        *hi |= 1u << (col - 20);
}

static int centered(const char *s, int sx)
{
    return VIS_COL + ((VIS_W - fb_text_width(s, sx)) >> 1);
}

static void draw_title(int stage)
{
    if (stage >= 1 && drawn < 1)
        fb_text(centered("MEH-GA", 1), 8, "MEH-GA", 1, 5);
    if (stage >= 2 && drawn < 2)
        fb_text(centered("MAN", 2), 38, "MAN", 2, 6);
    if (stage > drawn)
        drawn = (uint8_t)stage;
}

static void enter_vista(int skipped, unsigned tick)
{
    phase = 1;
    vtick = skipped ? V_READY : 0;
    drawn = 0;
    prompt_on = 0;
    fb_clear();
    if (skipped)
        draw_title(2);
    vista_draw_all(tick);
}

void intro_enter(void)
{
    int t, c;

    phase = 0;
    scroll = SCROLL_START;
    hold = 0;
    vtick = 0;
    drawn = 0;
    cache_row = -1;
    cache_tier = -1;
    cache_lo = 0;
    cache_hi = 0;
    for (t = 0; t < NUM_TIERS; t++) {
        body_lo[t] = 0;
        body_hi[t] = 0;
        for (c = tiers[t].c0; c <= tiers[t].c1; c++)
            add_cell(&body_lo[t], &body_hi[t], c);
    }
    fb_clear();
}

int intro_ready(void)
{
    return phase == 1 && vtick >= V_READY;
}

void intro_skip(void)
{
    if (phase == 0) {
        enter_vista(1, 0);
    } else if (vtick < V_READY) {
        vtick = V_READY;
        draw_title(2);
        vista_draw_all(0);
    }
}

/* ---- part 1 ---- */

/* Windows are 2 cells wide on a 3-cell pitch; a few are shut, scattered by a hash. */
static void open_windows(int row, int t)
{
    int cc, i = 0;

    cache_lo = 0;
    cache_hi = 0;
    for (cc = tiers[t].c0 + 1; cc + 1 <= tiers[t].c1 - 1; cc += 3, i++) {
        if (((row * 5 + i * 3 + t) & 3) != 0) {
            add_cell(&cache_lo, &cache_hi, cc);
            add_cell(&cache_lo, &cache_hi, cc + 1);
        }
    }
    cache_row = (int16_t)row;
    cache_tier = (int8_t)t;
}

/* The playfield cells of world line w, in two 20-cell halves. */
static void world_line(int w, uint32_t *lo, uint32_t *hi, unsigned tick)
{
    uint32_t l, h;
    int t;

    *lo = 0;
    *hi = 0;
    if (w < 0)
        return;
    if (w >= WORLD_GROUND) {
        *lo = 0xFFFFFu;
        *hi = 0xFFFFFu;
        return;
    }
    if (w < ROOF_Y) {                       /* the spire, with a blinking beacon */
        if (w >= 3 || (tick & 16u))
            *hi = 1u << (SPIRE_COL - 20);
        return;
    }

    for (t = NUM_TIERS - 1; t > 0 && w < tiers[t].y0; t--)
        ;
    l = body_lo[t];
    h = body_hi[t];
    if (w >= 380)
        l |= 0x0Cu;                         /* a low building at the left, cells 2-3 */
    if (w >= 340)
        h |= 0xF0000u;                      /* and a taller one at the right, 36-39 */

    if ((unsigned)((w & 7) - 2) < 4u) {     /* the 4 window lines of each 8-line floor */
        const int row = w >> 3;

        if (row != cache_row || t != cache_tier)
            open_windows(row, t);
        l &= ~cache_lo;
        h &= ~cache_hi;
    }
    *lo = l;
    *hi = h;
}

/* Black at altitude, a blue glow toward the street. */
static uint8_t pan_sky(int w)
{
    unsigned lum;

    if (w < 150)
        return 0x00u;
    lum = 1u + (unsigned)((w - 150) >> 6);
    if (lum > 7u)
        lum = 7u;
    return (uint8_t)(0x80u | (lum << 1));
}

static void pan_frame(unsigned tick)
{
    display_sprite_t hero;
    int y;

    if (scroll > SCROLL_END) {
        scroll = (int16_t)(scroll - (scroll - SCROLL_END > SLOW_ZONE ? PAN_FAST : PAN_SLOW));
        if (scroll < SCROLL_END)
            scroll = SCROLL_END;
    } else if (++hold >= PAN_HOLD) {
        enter_vista(0, tick);           /* cut to the vista, and let it draw this frame */
        vista_frame(tick);
        return;
    }

    for (y = 0; y <= LINES; y++) {
        display_bk_line(y, pan_sky(scroll + y));
        if (y < LINES) {
            uint32_t lo, hi;

            world_line(scroll + y, &lo, &hi, tick);
            display_pf_line(y, lo, hi);
        }
    }

    hero.x = 88;                            /* on the right half of the roof, facing out */
    hero.top = (int16_t)(ROOF_Y - 16 - scroll);
    hero.frame = ART_HERO_IDLE;
    hero.flip = 0;
    if (hero.top > -16 && hero.top < LINES)
        display_sprites(&hero, 1);
    else
        display_no_sprites();
    display_finish(PAN_PF);
}

/* ---- part 2 ---- */

/* The row of a 5-row font block (top line `top`, sy lines per row) that line y is in. */
static int font_row(int y, int top, int sy)
{
    int r = 0, t = top;

    while (r < 4 && y >= t + sy) {
        t += sy;
        r++;
    }
    return r;
}

static uint8_t vista_pf_color(int y)
{
    if (y >= V_CITY_TOP)
        return y < 150 ? 0x6Eu : (y < 172 ? 0x6Cu : 0x6Au);
    if (y >= 96)
        return 0x00u;                       /* the mountains are black */
    if (y >= 8 && y < 33)
        return ramp[font_row(y, 8, 5)];     /* MEH-GA */
    if (y >= 38 && y < 68)
        return ramp[font_row(y, 38, 6)];    /* MAN */
    return 0x0Eu;                           /* the prompt and the stars */
}

static uint8_t vista_bk(int y)
{
    unsigned lum;

    if (y < V_GLOW_TOP)
        return 0x00u;
    if (y < V_CITY_TOP) {
        lum = 1u + (unsigned)((y - V_GLOW_TOP) >> 2);
        return (uint8_t)(0x80u | ((lum > 7u ? 7u : lum) << 1));
    }
    return 0x00u;                           /* the city at night: only the lights show */
}

/* The picture's cells on screen line y, two 20-cell halves, before the text. */
static void vista_line(int y, unsigned tick, uint32_t *lo, uint32_t *hi)
{
    uint32_t l = 0, h = 0;
    int c, i;

    if (y < V_GLOW_TOP) {                   /* a few stars, twinkling */
        if (y >= 2 && ((y * 13) & 7) == 3 && ((y + (int)(tick >> 4)) & 3) != 0)
            add_cell(&l, &h, 4 + ((y * 29) & 31));
    } else if (y >= V_MT_TOP && y < V_CITY_TOP) {
        for (c = 0; c < 40; c++)
            if (y >= ridge[c])
                add_cell(&l, &h, c);
        h |= 0x10000u;                      /* a radio mast at cell 36 */
    } else if (y >= V_CITY_TOP) {
        for (i = 0; i < 5; i++) {
            const int d = y - city[i].top;

            if (d < 0)
                continue;
            if (d == 0) {                   /* the roof line */
                for (c = city[i].c0; c <= city[i].c1; c++)
                    add_cell(&l, &h, c);
                continue;
            }
            add_cell(&l, &h, city[i].c0);   /* the walls */
            add_cell(&l, &h, city[i].c1);
            if ((d & 3) >= 2)               /* two-line windows, a few of them dark */
                for (c = city[i].c0 + 1; c < city[i].c1; c++)
                    if (((c * 7 + (d >> 2) * 3 + city[i].c0) & 3) != 0)
                        add_cell(&l, &h, c);
        }
        if (y < V_ROOF) {                   /* a vent stack at the left end of the roof */
            h |= y >= 126 ? 0x30u : 0x78u;
            h |= 0x10000u;                  /* the mast continues */
        } else {                            /* the rooftop: solid, a groove, dark slats */
            if (y != V_ROOF + 3)
                h |= 0xFFFF0u;
            if (y >= V_ROOF + 12)
                h &= ~((1u << 10) | (1u << 13));
        }
    }
    *lo = l;
    *hi = h;
}

/* Redraw the playfield cells of lines y0..y1-1 (the framebuffer text goes on top). */
static void vista_draw_lines(int y0, int y1, unsigned tick)
{
    int y;

    for (y = y0; y < y1; y++) {
        uint32_t lo, hi, fl, fh;

        vista_line(y, tick, &lo, &hi);
        fb_line(y, &fl, &fh);
        if (y < V_GLOW_TOP && (fl | fh) != 0u) {
            lo = 0;                         /* no stars among the letters */
            hi = 0;
        }
        display_pf_line(y, lo ^ fl, hi ^ fh);
    }
}

/* The whole picture. The tables persist, so this runs once per change of scene. */
static void vista_draw_all(unsigned tick)
{
    int y;

    for (y = 0; y <= LINES; y++) {
        display_bk_line(y, vista_bk(y));
        display_pfcolor_line(y, vista_pf_color(y));
    }
    vista_draw_lines(0, LINES, tick);
}

#define STAR_LINES 8                        /* stars sit on lines 7, 15, ... 87 */

static void vista_frame(unsigned tick)
{
    display_sprite_t hero;
    int y;

    vtick++;
    if (vtick == V_TITLE_1) {
        draw_title(1);
        vista_draw_lines(8, 33, tick);
    } else if (vtick == V_TITLE_2) {
        draw_title(2);
        vista_draw_lines(38, 68, tick);
    }

    if (intro_ready()) {                    /* blinking prompt: on 32 frames, off 32 */
        const int on = !(tick & 32u);

        if (on != prompt_on) {
            prompt_on = (int8_t)on;
            fb_fill(VIS_COL, 76, VIS_W, 15, 0);
            if (on)
                fb_text(centered("START", 1), 76, "START", 1, 3);
            vista_draw_lines(76, 91, tick);
        }
    }

    if ((tick & 15u) == 0)                  /* the stars twinkle every 16 frames */
        for (y = 7; y < V_GLOW_TOP; y += STAR_LINES)
            vista_draw_lines(y, y + 1, tick);

    hero.x = V_HERO_X;                      /* standing on the roof, looking out at the city */
    hero.top = V_ROOF - 16;
    hero.frame = ART_HERO_IDLE;
    hero.flip = 1;
    display_sprites(&hero, 1);
    display_finish_scene();
}

void intro_frame(unsigned tick)
{
    if (phase == 0)
        pan_frame(tick);
    else
        vista_frame(tick);
}
