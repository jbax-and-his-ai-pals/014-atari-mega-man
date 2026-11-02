/*
 * Test-room geometry, shared by the renderer and the physics so what is drawn
 * is what the hero collides with. Units: pixels (0-159) and scanlines (0-191).
 * Playfield blocks are 4 px wide, so X edges are multiples of 4.
 */
#ifndef LEVEL_H
#define LEVEL_H

/* Full-width floor from this scanline to the bottom of the screen. */
#define FLOOR_TOP   160

/* One-way ledge: the hero lands on it from above and can jump up through it. */
#define LEDGE_TOP   128
#define LEDGE_ROWS  8
#define LEDGE_X0    32              /* first pixel */
#define LEDGE_X1    128             /* one past the last pixel */

#define HERO_W      16
#define HERO_H      16

#endif /* LEVEL_H */
