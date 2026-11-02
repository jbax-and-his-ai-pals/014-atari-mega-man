#ifndef PHYSICS_H
#define PHYSICS_H

void physics_init(void);

/* Put the hero back at the level's spawn point, at rest. */
void physics_respawn(void);

/* Advance one frame. pad is a PAD_* bit set from input.h. */
void physics_step(unsigned pad);

/* Top scanline of the hero. May be outside 0..LINES-1 while it is off screen. */
int physics_hero_row(void);

/* Left pixel of the hero in the level, 0..LEVEL_W_PX-HERO_W. */
int physics_hero_x(void);

#endif /* PHYSICS_H */
