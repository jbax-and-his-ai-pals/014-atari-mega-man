/*
 * Enemies: simple walkers and flyers loaded from level_enemies[]
 * (level text markers 'W' and 'F', see tools/level2c.py).
 * Positions are top-left corners in level pixels.
 */
#ifndef ENEMIES_H
#define ENEMIES_H

#define ENEMY_W 16
#define ENEMY_H 16

void enemies_init(void);                 /* (re)load every enemy from level_enemies, all alive */
void enemies_step(void);                 /* advance one frame */
int  enemies_count(void);                /* number of enemies (alive or not) */

/* Enemy i: returns 0 if dead/inactive, else 1 and its top-left in level pixels
 * and its type (ENEMY_WALKER / ENEMY_FLYER). */
int  enemies_get(int i, int *x, int *y, unsigned *type);

/* 1 if the hero box (level pixels) touches any live enemy; boxes are shrunk by
 * 2 px on each side for fairness. */
int  enemies_touch(int hx, int hy, int hw, int hh);
/* The way enemy i is heading: +1 right, -1 left. */
int  enemies_dir(int i);
/* Index of the first live enemy whose full box overlaps the given box, or -1. */
int  enemies_hit(int x, int y, int w, int h);
/* Remove enemy i until the next enemies_init(). */
void enemies_kill(int i);

#endif /* ENEMIES_H */
