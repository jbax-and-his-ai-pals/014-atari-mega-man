#ifndef PHYSICS_H
#define PHYSICS_H

void physics_init(void);
void physics_step(void);

/* Top scanline of the hero. May be outside 0..LINES-1 while it is off screen. */
int physics_hero_row(void);

#endif /* PHYSICS_H */
