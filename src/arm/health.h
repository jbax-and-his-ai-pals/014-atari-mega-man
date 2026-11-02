#ifndef HEALTH_H
#define HEALTH_H

#define HEALTH_MAX 3

/* Full health, no invulnerability. */
void health_init(void);

/* Count down invulnerability; call once per frame. */
void health_step(void);

/* 1 while the hero cannot be hurt (just after a hit). */
int health_invulnerable(void);

/* 1 on frames when the hero should be hidden, so it blinks while invulnerable. */
int health_blink(void);

/* Lose one point and become invulnerable. Returns 1 if that was the last point. */
int health_hurt(void);

int health_points(void);

#endif /* HEALTH_H */
