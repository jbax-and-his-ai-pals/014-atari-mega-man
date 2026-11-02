#ifndef PHYSICS_H
#define PHYSICS_H

void physics_init(void);

/* Advance one frame. pad is a PAD_* bit set from input.h. */
void physics_step(unsigned pad);

/* Top scanline of the hero. May be outside 0..LINES-1 while it is off screen. */
int physics_hero_row(void);

/* Left pixel of the hero, 0..160-HERO_W. */
int physics_hero_x(void);

#endif /* PHYSICS_H */
