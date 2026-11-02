/*
 * Saws, crushers and belts. Positions are level pixels. Everything here is drawn
 * as playfield cells (display_pf_cell), so it is free of the two-sprite limit.
 *
 * Saw: 16x16 (4 cells by 16 lines), a spinning toothed disc. Horizontal saws
 * ('O') patrol until they meet a solid tile, vertical ones ('U') likewise.
 * Crusher ('P'): a 16-wide head on a shaft that drops from its anchor, waits, and
 * rises again, on a 128-frame cycle that starts at a different point for each.
 * Belt: a run of solid tiles that carries whatever stands on it, drawn with
 * gaps that crawl in the direction it moves.
 *
 * Every static is assigned in hazards_init(): RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "cdfj.h"
#include "display_list.h"
#include "hazards.h"
#include "level.h"

#define MAX_HAZARDS   24
#define SAW_SIZE      16
#define SAW_SPEED     2             /* px per frame */
#define CRUSH_CYCLE   128
#define CRUSH_REACH   40            /* how far the head drops below its anchor */
#define CRUSH_HEAD_H  8
#define SCREEN_CELLS  40

/* The saw's picture: 4 cells wide, 16 lines tall, bit 3 leftmost; two spin frames. */
static const uint8_t saw_art[2][16] = {
    { 0x6, 0xF, 0x7, 0xF, 0xF, 0x7, 0x9, 0x9, 0x9, 0x9, 0xE, 0xF, 0xF, 0xE, 0xF, 0x6 },
    { 0x6, 0xF, 0xE, 0xF, 0xE, 0xF, 0x9, 0x9, 0x9, 0x9, 0x7, 0xF, 0x7, 0xF, 0xF, 0x6 },
};

static int16_t h_x[MAX_HAZARDS];
static int16_t h_y[MAX_HAZARDS];
static int8_t h_dir[MAX_HAZARDS];
static uint8_t h_type[MAX_HAZARDS];
static uint16_t ticks;

/* No libc on the cart: keep gcc from turning the clearing loop into memset(). */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void hazards_init(void)
{
    int i;

    ticks = 0;
    for (i = 0; i < MAX_HAZARDS; i++) {
        h_x[i] = 0;
        h_y[i] = 0;
        h_dir[i] = 1;
        h_type[i] = 0;
    }
    for (i = 0; i < (int)LEVEL_NUM_HAZARDS && i < MAX_HAZARDS; i++) {
        h_x[i] = level_hazards[i].x;
        h_y[i] = level_hazards[i].y;
        h_type[i] = level_hazards[i].type;
    }
}

/* Is any tile under the pixel rectangle (x, y, SAW_SIZE square) solid? */
static int saw_blocked(int x, int y)
{
    int r, c;

    for (r = y >> 3; r <= (y + SAW_SIZE - 1) >> 3; r++)
        for (c = x >> 2; c <= (x + SAW_SIZE - 1) >> 2; c++)
            if (level_solid(c, r))
                return 1;
    return 0;
}

void hazards_step(void)
{
    int i;

    ticks++;
    for (i = 0; i < (int)LEVEL_NUM_HAZARDS && i < MAX_HAZARDS; i++) {
        if (h_type[i] == HAZ_SAW_H) {
            const int nx = h_x[i] + h_dir[i] * SAW_SPEED;

            if (saw_blocked(nx, h_y[i]))
                h_dir[i] = (int8_t)-h_dir[i];
            else
                h_x[i] = (int16_t)nx;
        } else if (h_type[i] == HAZ_SAW_V) {
            const int ny = h_y[i] + h_dir[i] * SAW_SPEED;

            if (ny < 0 || ny + SAW_SIZE > (int)LINES || saw_blocked(h_x[i], ny))
                h_dir[i] = (int8_t)-h_dir[i];
            else
                h_y[i] = (int16_t)ny;
        }
    }
}

/* How far crusher i's head is below its anchor, 0..CRUSH_REACH. */
static int crush_extent(int i)
{
    const int t = (ticks + (unsigned)(h_x[i] >> 2) * 37u) & (CRUSH_CYCLE - 1);

    if (t < 60)
        return 0;                           /* retracted */
    if (t < 64)
        return (t - 59) * (CRUSH_REACH / 4);    /* drops fast */
    if (t < 96)
        return CRUSH_REACH;                 /* holds */
    return CRUSH_REACH - (((t - 96) * 5) >> 2);  /* rises slowly */
}

int hazards_hit(int x, int y, int w, int h)
{
    int i;

    for (i = 0; i < (int)LEVEL_NUM_HAZARDS && i < MAX_HAZARDS; i++) {
        int hx = h_x[i], hy = h_y[i], hw = SAW_SIZE, hh = SAW_SIZE;

        if (h_type[i] == HAZ_CRUSHER) {
            hy += crush_extent(i);
            hh = CRUSH_HEAD_H;
        }
        if (x < hx + hw && x + w > hx && y < hy + hh && y + h > hy)
            return 1;
    }
    return 0;
}

int hazards_belt(int x, int y)
{
    int i;

    for (i = 0; i < (int)LEVEL_NUM_CONVEYORS; i++)
        if (level_conveyors[i].y == y && x >= level_conveyors[i].x
                && x < level_conveyors[i].x + (int)level_conveyors[i].w)
            return level_conveyors[i].dir;
    return 0;
}

/* Draw cells c0..c0+3 of one line from a 4-bit mask (bit 3 leftmost). */
static void draw_mask4(int line, int c0, unsigned mask)
{
    int k;

    if (line < 0 || line >= (int)LINES)
        return;
    for (k = 0; k < 4; k++)
        if (((mask >> (3 - k)) & 1u) && c0 + k >= 0 && c0 + k < SCREEN_CELLS)
            display_pf_cell(line, c0 + k, 1);
}

static void draw_belts(int cam)
{
    const unsigned phase = ticks >> 2;
    int i, c, k;

    for (i = 0; i < (int)LEVEL_NUM_CONVEYORS; i++) {
        const int first = (level_conveyors[i].x - cam) >> 2;
        const int count = level_conveyors[i].w >> 2;

        if (first + count <= 0 || first >= SCREEN_CELLS)
            continue;
        for (c = 0; c < count; c++) {
            const int cell = first + c;

            if (cell < 0 || cell >= SCREEN_CELLS)
                continue;
            /* A gap in every fourth cell of two lines, crawling along the belt. */
            if (((unsigned)(c - level_conveyors[i].dir * (int)phase) & 3u) == 0)
                for (k = 2; k < 4; k++)
                    display_pf_cell(level_conveyors[i].y + k, cell, 0);
        }
    }
}

void hazards_draw(int cam)
{
    int i, k;

    draw_belts(cam);
    for (i = 0; i < (int)LEVEL_NUM_HAZARDS && i < MAX_HAZARDS; i++) {
        const int c0 = (h_x[i] - cam) >> 2;

        if (c0 <= -4 || c0 >= SCREEN_CELLS)
            continue;
        if (h_type[i] == HAZ_CRUSHER) {
            const int ext = crush_extent(i);

            for (k = 0; k < ext; k++)       /* the shaft */
                draw_mask4(h_y[i] + k, c0, 0x6u);
            for (k = 0; k < CRUSH_HEAD_H; k++)  /* the head, with a slit */
                draw_mask4(h_y[i] + ext + k, c0, k == 3 ? 0x9u : 0xFu);
        } else {
            const uint8_t *art = saw_art[(ticks >> 1) & 1u];

            for (k = 0; k < SAW_SIZE; k++)
                draw_mask4(h_y[i] + k, c0, art[k]);
        }
    }
}
