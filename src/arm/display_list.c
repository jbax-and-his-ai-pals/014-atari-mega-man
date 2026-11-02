/*
 * Builds the per-scanline tables the 6507 kernel streams in.
 *
 * Playfield: a sky-colored background, a floating ledge, and a full-width
 * floor, with the geometry from level.h. The playfield is asymmetric: the left
 * and right 20 bits are stored separately.
 *
 * Hero: a 16x16 test sprite (GRP0 + GRP1) that cycles its color. Its X goes to
 * the 6507 through STREAM_HEROX; its rows are placed in the GRP tables here.
 *
 * The whole table is rebuilt every frame, so nothing here depends on state
 * left over from boot. The sky and floor rows come from constants, which keeps
 * the work inside the VBLANK budget.
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

/* PosObject in kernel.asm places a sprite this many pixels left of the value
 * it is given; measured with tools/snap.py. */
#define HERO_X_TRIM 3

/* Ledge as 40 playfield bits (bit n covers pixels 4n..4n+3). */
#define LEDGE_MASK  (((1ull << ((LEDGE_X1 - LEDGE_X0) / 4)) - 1) << (LEDGE_X0 / 4))

/* Playfield bytes for a row with all 20 bits set in one half (PF0, PF1, PF2). */
static const uint8_t full_half[3] = { 0xF0, 0xFF, 0xFF };
static const uint8_t empty_half[3] = { 0x00, 0x00, 0x00 };

/* High byte -> GRP0 (pixels 0-7), low byte -> GRP1 (pixels 8-15). */
static const uint16_t hero_rows[HERO_ROWS] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x7FFE, 0xFFFF, 0xE7E7, 0xE7E7, 0xFFFF,
    0xFFFF, 0xFFFF, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC, 0x1998, 0x1998
};

static uint32_t frame;

/*
 * Pack playfield bits base..base+19 (leftmost pixel first) into PF0, PF1, PF2.
 * PF0 uses D4-D7 with D4 leftmost, PF1 uses D7-D0 with D7 leftmost,
 * PF2 uses D0-D7 with D0 leftmost.
 */
static void pack_half(uint64_t m, unsigned base, uint8_t out[3])
{
    uint8_t pf0 = 0, pf1 = 0, pf2 = 0;
    unsigned i;

    for (i = 0; i < 4; i++)
        if ((m >> (base + i)) & 1u)
            pf0 |= (uint8_t)(1u << (4 + i));
    for (i = 0; i < 8; i++)
        if ((m >> (base + 4 + i)) & 1u)
            pf1 |= (uint8_t)(1u << (7 - i));
    for (i = 0; i < 8; i++)
        if ((m >> (base + 12 + i)) & 1u)
            pf2 |= (uint8_t)(1u << i);

    out[0] = pf0;
    out[1] = pf1;
    out[2] = pf2;
}

static void dd_put(unsigned off, uint8_t v)
{
    CDF_DD[off] = v;
}

/* Write one scanline's six playfield bytes. */
static void pf_put(unsigned line, const uint8_t left[3], const uint8_t right[3])
{
    dd_put(DD_PF0L + line, left[0]);
    dd_put(DD_PF1L + line, left[1]);
    dd_put(DD_PF2L + line, left[2]);
    dd_put(DD_PF0R + line, right[0]);
    dd_put(DD_PF1R + line, right[1]);
    dd_put(DD_PF2R + line, right[2]);
}

void display_build(void)
{
    const int hero_top = physics_hero_row();
    uint8_t ledge_left[3], ledge_right[3];
    unsigned line;
    unsigned n;

    pack_half(LEDGE_MASK, 0, ledge_left);
    pack_half(LEDGE_MASK, 20, ledge_right);

    for (line = 0; line < LINES; line++) {
        int rel = (int)line - hero_top;
        uint8_t g0 = 0, g1 = 0, c = 0;

        if (line >= LEDGE_TOP && line < LEDGE_TOP + LEDGE_ROWS)
            pf_put(line, ledge_left, ledge_right);
        else if (line >= FLOOR_TOP)
            pf_put(line, full_half, full_half);
        else
            pf_put(line, empty_half, empty_half);

        /* Sky: blue, getting brighter toward the floor. */
        dd_put(DD_BK + line, (uint8_t)(0x90u | ((2u + (line >> 5)) << 1)));

        if (rel >= 0 && rel < (int)HERO_ROWS) {
            unsigned t = ((unsigned)rel + (unsigned)frame) >> 1;
            uint16_t row = hero_rows[rel];

            g0 = (uint8_t)(row >> 8);
            g1 = (uint8_t)(row & 0xFFu);
            c  = (uint8_t)(0x40u | ((t & 7u) << 1));
        }
        dd_put(DD_GRP0 + line, g0);
        dd_put(DD_GRP1 + line, g1);
        dd_put(DD_COLUP0 + line, c);
        dd_put(DD_COLUP1 + line, c);
    }

    dd_put(DD_COLUPF, 0x1Eu);       /* platform color: gold */
    dd_put(DD_HEROX, (uint8_t)(physics_hero_x() + HERO_X_TRIM));

    /* Point each stream at its table, and step one byte per fetch. */
    for (n = 0; n < NUM_STREAMS; n++) {
        cdf_stream_set(n, stream_base[n]);
        cdf_stream_step1(n);
    }

    frame++;
}
