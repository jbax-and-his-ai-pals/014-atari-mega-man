/*
 * Builds the per-scanline tables the 6507 kernel streams in.
 *
 * Playfield: a 40-column window into the tile map (level.h), one tile row per
 * 8 scanlines, over a sky gradient. The playfield is asymmetric: the left and
 * right 20 bits are stored separately.
 *
 * Camera: follows the hero. The playfield can only move in whole 4 px columns,
 * so the camera snaps to a column; sprites still move one pixel at a time.
 *
 * Hero: a 16x16 test sprite (GRP0 + GRP1) that cycles its color. Its screen X
 * goes to the 6507 through STREAM_HEROX; its rows are placed in the GRP tables.
 *
 * The whole table is rebuilt every frame, so nothing here depends on state
 * left over from boot.
 */
#include <stdint.h>
#include "cdfj.h"
#include "display_list.h"
#include "level.h"
#include "physics.h"

/* Stream n -> display-data table. Order matches STREAM_* in layout.inc. */
static const uint16_t stream_base[NUM_STREAMS] = {
    DD_PF0L, DD_PF1L, DD_PF2L, DD_PF0R, DD_PF1R, DD_PF2R,
    DD_BK, DD_GRP0, DD_GRP1, DD_COLUP0, DD_COLUP1, DD_COLUPF,
    DD_HEROX
};

#define HERO_ROWS   ((unsigned)HERO_H)
#define SCREEN_COLS 40
#define CAMERA_MAX  (LEVEL_W_PX - SCREEN_COLS * TILE_W)
#define HERO_SCREEN_X 72            /* where the camera keeps the hero, mid-level */

/* PosObject in kernel.asm places a sprite this many pixels left of the value
 * it is given; measured with tools/snap.py. */
#define HERO_X_TRIM 3

/* Bit-reverse a byte: PF1 shows its bits D7 first, the map stores leftmost first. */
#define R2(n) n, n + 2 * 64, n + 1 * 64, n + 3 * 64
#define R4(n) R2(n), R2(n + 2 * 16), R2(n + 1 * 16), R2(n + 3 * 16)
#define R6(n) R4(n), R4(n + 2 * 4), R4(n + 1 * 4), R4(n + 3 * 4)
static const uint8_t rev8[256] = { R6(0), R6(2), R6(1), R6(3) };

/* High byte -> GRP0 (pixels 0-7), low byte -> GRP1 (pixels 8-15). */
static const uint16_t hero_rows[HERO_ROWS] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x7FFE, 0xFFFF, 0xE7E7, 0xE7E7, 0xFFFF,
    0xFFFF, 0xFFFF, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC, 0x1998, 0x1998
};

static uint32_t frame;

/*
 * Pack 20 playfield bits (bit 0 = leftmost pixel) into PF0, PF1, PF2.
 * PF0 uses D4-D7 with D4 leftmost, PF1 uses D7-D0 with D7 leftmost,
 * PF2 uses D0-D7 with D0 leftmost.
 */
static void pack_half(uint32_t bits, uint8_t out[3])
{
    out[0] = (uint8_t)((bits & 0x0Fu) << 4);
    out[1] = rev8[(bits >> 4) & 0xFFu];
    out[2] = (uint8_t)((bits >> 12) & 0xFFu);
}

static void dd_put(unsigned off, uint8_t v)
{
    CDF_DD[off] = v;
}

/* Left pixel of the screen in the level, snapped to a whole column. */
static int camera_x(int hero_x)
{
    int cam = hero_x - HERO_SCREEN_X;

    if (cam < 0)
        cam = 0;
    if (cam > CAMERA_MAX)
        cam = CAMERA_MAX;
    return cam & ~(TILE_W - 1);
}

void display_build(void)
{
    const int hero_x = physics_hero_x();
    const int hero_top = physics_hero_row();
    const int cam = camera_x(hero_x);
    const unsigned col0 = (unsigned)cam / TILE_W;
    unsigned row, line = 0;
    unsigned n;

    for (row = 0; row < LEVEL_ROWS; row++) {
        uint8_t left[3], right[3];
        unsigned k;

        pack_half(level_row_bits(row, col0), left);
        pack_half(level_row_bits(row, col0 + 20), right);

        for (k = 0; k < TILE_H; k++, line++) {
            int rel = (int)line - hero_top;
            uint8_t g0 = 0, g1 = 0, c = 0;

            dd_put(DD_PF0L + line, left[0]);
            dd_put(DD_PF1L + line, left[1]);
            dd_put(DD_PF2L + line, left[2]);
            dd_put(DD_PF0R + line, right[0]);
            dd_put(DD_PF1R + line, right[1]);
            dd_put(DD_PF2R + line, right[2]);

            /* Sky: blue, getting brighter toward the bottom. */
            dd_put(DD_BK + line, (uint8_t)(0x90u | ((2u + (line >> 5)) << 1)));

            if (rel >= 0 && rel < (int)HERO_ROWS) {
                unsigned t = ((unsigned)rel + (unsigned)frame) >> 1;
                uint16_t bits = hero_rows[rel];

                g0 = (uint8_t)(bits >> 8);
                g1 = (uint8_t)(bits & 0xFFu);
                c  = (uint8_t)(0x40u | ((t & 7u) << 1));
            }
            dd_put(DD_GRP0 + line, g0);
            dd_put(DD_GRP1 + line, g1);
            dd_put(DD_COLUP0 + line, c);
            dd_put(DD_COLUP1 + line, c);
        }
    }

    dd_put(DD_COLUPF, 0x1Eu);       /* platform color: gold */
    dd_put(DD_HEROX, (uint8_t)(hero_x - cam + HERO_X_TRIM));

    /* Point each stream at its table, and step one byte per fetch. */
    for (n = 0; n < NUM_STREAMS; n++) {
        cdf_stream_set(n, stream_base[n]);
        cdf_stream_step1(n);
    }

    frame++;
}
