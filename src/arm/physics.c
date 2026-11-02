/*
 * Hero physics in 8.8 fixed point. The ARM7 has no FPU and no hardware divide,
 * so there are no floats and no / or % in this file.
 *
 * Left/right walks, fire jumps. Releasing fire early cuts the jump short.
 * The floor is solid; the ledge is one-way (land from above, jump up through).
 */
#include <stdint.h>
#include "input.h"
#include "level.h"
#include "physics.h"

#define GRAVITY     0x0040          /* +0.25 scanlines/frame^2 */
#define MAX_FALL    0x0400          /* 4 scanlines/frame */
#define JUMP_SPEED  (-0x0480)       /* -4.5 scanlines/frame: rises about 40 lines */
#define JUMP_CUT    (-0x0100)       /* rise speed cap once fire is released */
#define WALK_SPEED  0x0140          /* 1.25 px/frame */
#define X_MAX       ((160 - HERO_W) << 8)

static int32_t hero_x;              /* left of hero, 8.8 pixels */
static int32_t hero_y;              /* top of hero, 8.8 scanlines */
static int32_t hero_vy;             /* 8.8 scanlines per frame */
static unsigned on_ground;
static unsigned prev_pad;

void physics_init(void)
{
    hero_x    = 72 << 8;
    hero_y    = 60 << 8;
    hero_vy   = 0;
    on_ground = 0;
    prev_pad  = 0;
}

/* Does the hero (left pixel px) overlap pixels x0..x1-1? */
static int overlaps(int px, int x0, int x1)
{
    return px + HERO_W > x0 && px < x1;
}

void physics_step(unsigned pad)
{
    const unsigned pressed = pad & ~prev_pad;
    int32_t feet_before, feet_after;

    if (pad & PAD_LEFT)
        hero_x -= WALK_SPEED;
    if (pad & PAD_RIGHT)
        hero_x += WALK_SPEED;
    if (hero_x < 0)
        hero_x = 0;
    if (hero_x > X_MAX)
        hero_x = X_MAX;

    if ((pressed & PAD_FIRE) && on_ground)
        hero_vy = JUMP_SPEED;
    if (!(pad & PAD_FIRE) && hero_vy < JUMP_CUT)
        hero_vy = JUMP_CUT;

    hero_vy += GRAVITY;
    if (hero_vy > MAX_FALL)
        hero_vy = MAX_FALL;

    feet_before = hero_y + (HERO_H << 8);
    hero_y += hero_vy;
    feet_after = hero_y + (HERO_H << 8);
    on_ground = 0;

    if (hero_vy >= 0) {
        const int px = (int)(hero_x >> 8);

        if (feet_before <= (LEDGE_TOP << 8) && feet_after >= (LEDGE_TOP << 8)
                && overlaps(px, LEDGE_X0, LEDGE_X1)) {
            hero_y = (LEDGE_TOP - HERO_H) << 8;
            hero_vy = 0;
            on_ground = 1;
        } else if (feet_after >= (FLOOR_TOP << 8)) {
            hero_y = (FLOOR_TOP - HERO_H) << 8;
            hero_vy = 0;
            on_ground = 1;
        }
    }

    prev_pad = pad;
}

int physics_hero_row(void)
{
    return (int)(hero_y >> 8);
}

int physics_hero_x(void)
{
    return (int)(hero_x >> 8);
}
