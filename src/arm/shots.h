#ifndef SHOTS_H
#define SHOTS_H

#define SHOT_W 4                    /* matches the 4 px missile width (NUSIZ0) */
#define SHOT_H 2

/* Remove every bullet. */
void shots_init(void);

/* Fire on a fresh press of PAD_FIRE, move bullets, and hit tiles and enemies. */
void shots_step(unsigned pad);

/* Live bullet number i (counting only live ones from 0): 1 and its top-left in level
 * pixels, or 0 once there are no more. Bullets are drawn as playfield cells. */
int shots_get(int i, int *x, int *y);

#endif /* SHOTS_H */
