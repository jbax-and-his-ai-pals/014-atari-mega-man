#ifndef PHYSICS_H
#define PHYSICS_H

void physics_init(void);

/* Put the hero back at the level's spawn point, at rest. */
void physics_respawn(void);

/* Advance one frame. pad is a PAD_* bit set from input.h. Returns 1 if the
 * hero fell out of the level this frame (and was respawned). */
int physics_step(unsigned pad);

/* Throw the hero away from a hit: dir is +1 (right) or -1 (left). Brief loss of control. */
void physics_knock(int dir);

/* Top scanline of the hero. May be outside 0..LINES-1 while it is off screen. */
int physics_hero_row(void);

/* Left pixel of the hero in the level, 0..LEVEL_W_PX-HERO_W. */
int physics_hero_x(void);

/* Which picture the hero should wear. */
enum { POSE_IDLE, POSE_RUN_A, POSE_RUN_P, POSE_RUN_B, POSE_JUMP };
int physics_hero_pose(void);

/* +1 facing right, -1 facing left (the last direction walked). */
int physics_hero_facing(void);

#endif /* PHYSICS_H */
