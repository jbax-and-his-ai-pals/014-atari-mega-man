/*
 * Stage hazards: saws, crushers and conveyor belts. All of them are drawn into the
 * playfield, so they cost no sprite and never flicker, however many are on screen.
 * They come from the level file (O, U, P and the > < belt tiles).
 */
#ifndef HAZARDS_H
#define HAZARDS_H

/* Put every hazard back at its start. */
void hazards_init(void);

/* Advance one frame. */
void hazards_step(void);

/* 1 if the box (level pixels) overlaps any saw or crusher head. */
int hazards_hit(int x, int y, int w, int h);

/* Direction of the conveyor belt under pixel (x, y): +1 right, -1 left, 0 none.
 * y is the top of the tile row, i.e. a standing hero's feet. */
int hazards_belt(int x, int y);

/* Draw them into the playfield tables, over the tile map. cam is the left edge of
 * the screen in the level. */
void hazards_draw(int cam);

#endif /* HAZARDS_H */
