/*
 * Hero physics in 8.8 fixed point. The ARM7 has no FPU and no hardware divide,
 * so there are no floats and no / or % in this file.
 *
 * Left/right walks, up jumps (fire shoots, see shots.c). Releasing up early
 * cuts the jump short.
 * Collision is against the tile map (level.h), one axis at a time. Speeds stay
 * below one tile per frame (4 px across, 8 lines down), so nothing tunnels.
 * Falling off the bottom of the screen respawns the hero.
 */
#include <stdint.h>
#include "input.h"
#include "level.h"
#include "physics.h"

#define GRAVITY     0x0040          /* +0.25 scanlines/frame^2 */
#define MAX_FALL    0x0400          /* 4 scanlines/frame */
#define JUMP_SPEED  (-0x0480)       /* -4.5 scanlines/frame: rises about 40 lines */
#define JUMP_CUT    (-0x0100)       /* rise speed cap once up is released */
#define WALK_SPEED  0x0140          /* 1.25 px/frame */
#define KNOCK_SPEED 0x0200          /* 2 px/frame away from what hit the hero */
#define KNOCK_FRAMES 8              /* so about 16 px */
#define KNOCK_HOP   (-0x0300)       /* small hop, -3 scanlines/frame */
#define X_MAX       ((LEVEL_W_PX - HERO_W) << 8)
#define RESPAWN_Y   ((int)LINES + HERO_H) /* top scanline that counts as fallen out */

static int32_t hero_x;              /* left of hero, 8.8 pixels in the level */
static int32_t hero_y;              /* top of hero, 8.8 scanlines */
static int32_t hero_vy;             /* 8.8 scanlines per frame */
static unsigned on_ground;
static unsigned prev_pad;
static int facing;                  /* +1 right, -1 left */
static int32_t knock_vx;            /* 8.8 px/frame while knocked back */
static unsigned knock_t;            /* frames of knockback left */

static void spawn(void)
{
    hero_x    = LEVEL_SPAWN_X << 8;
    hero_y    = LEVEL_SPAWN_Y << 8;
    hero_vy   = 0;
    on_ground = 0;
    facing    = 1;
    knock_vx  = 0;
    knock_t   = 0;
}

void physics_knock(int dir)
{
    knock_vx = dir * KNOCK_SPEED;
    knock_t  = KNOCK_FRAMES;
    hero_vy  = KNOCK_HOP;
}

void physics_init(void)
{
    spawn();
    prev_pad = 0;
}

void physics_respawn(void)
{
    spawn();
}

/* Any solid tile under the pixel rectangle x0..x1, y0..y1 (inclusive)? */
static int hits(int x0, int x1, int y0, int y1)
{
    int r, c;

    for (r = y0 >> 3; r <= (y1 >> 3); r++)
        for (c = x0 >> 2; c <= (x1 >> 2); c++)
            if (level_solid(c, r))
                return 1;
    return 0;
}

/* Move horizontally by dx (8.8 pixels, signed), stopping at walls. */
static void move_x(int32_t dx)
{
    int px, py;

    if (dx == 0)
        return;
    hero_x += dx;
    if (hero_x < 0)
        hero_x = 0;
    if (hero_x > X_MAX)
        hero_x = X_MAX;

    px = (int)(hero_x >> 8);
    py = (int)(hero_y >> 8);
    if (dx < 0) {
        if (hits(px, px, py, py + HERO_H - 1))
            hero_x = (((px >> 2) + 1) << 2) << 8;
    } else {
        if (hits(px + HERO_W - 1, px + HERO_W - 1, py, py + HERO_H - 1))
            hero_x = ((((px + HERO_W - 1) >> 2) << 2) - HERO_W) << 8;
    }
}

static void move_y(void)
{
    int px, py;

    hero_vy += GRAVITY;
    if (hero_vy > MAX_FALL)
        hero_vy = MAX_FALL;
    hero_y += hero_vy;

    px = (int)(hero_x >> 8);
    py = (int)(hero_y >> 8);
    on_ground = 0;

    if (hero_vy < 0) {
        /* Head in a tile: stop under it. */
        if (hits(px, px + HERO_W - 1, py, py)) {
            hero_y = (((py >> 3) + 1) << 3) << 8;
            hero_vy = 0;
        }
    } else if (hits(px, px + HERO_W - 1, py + HERO_H - 1, py + HERO_H - 1)) {
        /* Feet sank into a tile: stand on top of it. */
        hero_y = ((((py + HERO_H - 1) >> 3) << 3) - HERO_H) << 8;
        hero_vy = 0;
        on_ground = 1;
    } else if (hits(px, px + HERO_W - 1, py + HERO_H, py + HERO_H)) {
        /* Resting exactly on a tile. */
        hero_y = py << 8;
        hero_vy = 0;
        on_ground = 1;
    }
}

int physics_step(unsigned pad)
{
    const unsigned pressed = pad & ~prev_pad;
    const int knocked = knock_t != 0;
    int fell = 0;

    /* While knocked back the controls are ignored, and the hop is not cut short. */
    if (knocked) {
        knock_t--;
        move_x(knock_vx);
    } else if (pad & PAD_LEFT) {
        facing = -1;
        move_x(-WALK_SPEED);
    } else if (pad & PAD_RIGHT) {
        facing = 1;
        move_x(WALK_SPEED);
    }

    if (!knocked) {
        if ((pressed & PAD_UP) && on_ground)
            hero_vy = JUMP_SPEED;
        if (!(pad & PAD_UP) && hero_vy < JUMP_CUT)
            hero_vy = JUMP_CUT;
    }
    move_y();

    if ((hero_y >> 8) > RESPAWN_Y) {
        spawn();
        fell = 1;
    }

    prev_pad = pad;
    return fell;
}

int physics_hero_facing(void)
{
    return facing;
}

int physics_hero_row(void)
{
    return (int)(hero_y >> 8);
}

int physics_hero_x(void)
{
    return (int)(hero_x >> 8);
}
