/*
 * Tile map access, shared by the renderer and the physics so what is drawn is
 * what the hero collides with.
 *
 * The map comes from levels/stage1.txt through tools/level2c.py
 * (build/gen/level_data.h).
 * A tile is 4 px wide (one playfield bit) and 8 scanlines tall, so one screen
 * is 40 x 24 tiles. Row r, column c is bit (c & 7) of
 * level_bits[r * LEVEL_STRIDE + (c >> 3)]; a set bit is solid.
 */
#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>
#include "level_data.h"

#define TILE_W      4
#define TILE_H      8
#define LEVEL_W_PX  ((int)LEVEL_COLS * TILE_W)

#define HERO_W      16
#define HERO_H      16

/*
 * Is the tile at (col, row) solid? Left and right of the map is solid (the
 * level's side walls); above and below it is empty, so the hero can jump off
 * the top of the screen and fall into pits.
 */
static inline int level_solid(int col, int row)
{
    if (row < 0 || row >= (int)LEVEL_ROWS)
        return 0;
    if (col < 0 || col >= (int)LEVEL_COLS)
        return 1;
    return (level_bits[row * LEVEL_STRIDE + ((unsigned)col >> 3)] >> (col & 7)) & 1u;
}

/*
 * At least 25 bits of tile row `row` starting at column `col` (bit 0 = col).
 * Needs col + 24 < LEVEL_COLS + 32; the 4 padding bytes per row cover the end.
 */
static inline uint32_t level_row_bits(unsigned row, unsigned col)
{
    const uint8_t *p = &level_bits[row * LEVEL_STRIDE + (col >> 3)];
    const uint32_t w = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    return w >> (col & 7u);
}

#endif /* LEVEL_H */
