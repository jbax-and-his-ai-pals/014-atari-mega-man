/*
 * Hero physics in 8.8 fixed point. The ARM7 has no FPU and no hardware divide,
 * so there are no floats and no / or % in this file.
 */
#include <stdint.h>
#include "physics.h"

#define GRAVITY     0x0040          /* +0.25 scanlines/frame^2 */
#define JUMP_SPEED  (-0x0400)       /* -4 scanlines/frame on each bounce */
#define GROUND_TOP  (144 << 8)      /* hero rests on the floor at line 160 */

static int32_t hero_y;              /* top of hero, 8.8 scanlines */
static int32_t hero_vy;             /* 8.8 scanlines per frame */

void physics_init(void)
{
    hero_y  = 60 << 8;
    hero_vy = 0;
}

void physics_step(void)
{
    hero_vy += GRAVITY;
    hero_y  += hero_vy;
    if (hero_y >= GROUND_TOP) {
        hero_y  = GROUND_TOP;
        hero_vy = JUMP_SPEED;
    }
}

int physics_hero_row(void)
{
    return (int)(hero_y >> 8);
}
