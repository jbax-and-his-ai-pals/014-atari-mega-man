#ifndef SHOTS_H
#define SHOTS_H

#define SHOT_W 4                    /* matches the 4 px missile width (NUSIZ0) */
#define SHOT_H 2

/* Remove every bullet. */
void shots_init(void);

/* Fire on a fresh press of PAD_FIRE, move bullets, and hit tiles and enemies. */
void shots_step(unsigned pad);

/*
 * The bullet to draw this frame (there is one missile, so two live bullets take
 * turns). Returns 0 if none, else 1 and its top-left in level pixels.
 */
int shots_pick(int *x, int *y);

#endif /* SHOTS_H */
