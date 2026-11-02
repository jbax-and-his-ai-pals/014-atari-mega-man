/*
 * The hero's bullets: short horizontal shots fired from the hero's middle in
 * the direction it faces. At most two at once, with a short cooldown. A bullet
 * ends when it hits a solid tile or an enemy (which it removes), or after it
 * has flown a screen width.
 *
 * Every static is assigned in shots_init(): RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "enemies.h"
#include "input.h"
#include "level.h"
#include "physics.h"
#include "shots.h"

#define SHOT_MAX      2
#define SHOT_SPEED    4             /* px/frame */
#define SHOT_RANGE    160           /* px flown before it vanishes */
#define SHOT_COOLDOWN 8             /* frames between shots */
#define GUN_Y         8             /* lines below the hero's top */

static int16_t s_x[SHOT_MAX];
static int16_t s_y[SHOT_MAX];
static int8_t s_dir[SHOT_MAX];      /* 0 = unused, else +1 / -1 */
static int16_t s_left[SHOT_MAX];    /* px still to fly */
static uint8_t cooldown;
static uint8_t prev_fire;
static uint8_t turn;                /* which bullet shots_pick shows next */

void shots_init(void)
{
    int i;

    for (i = 0; i < SHOT_MAX; i++) {
        s_x[i] = 0;
        s_y[i] = 0;
        s_dir[i] = 0;
        s_left[i] = 0;
    }
    cooldown = 0;
    prev_fire = 1;                  /* a held button does not fire on respawn */
    turn = 0;
}

static void fire(void)
{
    const int dir = physics_hero_facing();
    const int hx = physics_hero_x();
    int i;

    for (i = 0; i < SHOT_MAX; i++) {
        if (s_dir[i] == 0) {
            s_dir[i] = (int8_t)dir;
            s_x[i] = (int16_t)(dir > 0 ? hx + HERO_W : hx - SHOT_W);
            s_y[i] = (int16_t)(physics_hero_row() + GUN_Y);
            s_left[i] = SHOT_RANGE;
            cooldown = SHOT_COOLDOWN;
            return;
        }
    }
}

void shots_step(unsigned pad)
{
    const unsigned fire_now = (pad & PAD_FIRE) ? 1u : 0u;
    int i;

    if (cooldown)
        cooldown--;
    if (fire_now && !prev_fire && cooldown == 0)
        fire();
    prev_fire = (uint8_t)fire_now;

    for (i = 0; i < SHOT_MAX; i++) {
        int x, lead, hit;

        if (s_dir[i] == 0)
            continue;
        x = s_x[i] + s_dir[i] * SHOT_SPEED;
        s_left[i] = (int16_t)(s_left[i] - SHOT_SPEED);
        lead = s_dir[i] > 0 ? x + SHOT_W - 1 : x;

        if (s_left[i] <= 0 || x < 0 || x > LEVEL_W_PX - SHOT_W
                || level_solid(lead >> 2, s_y[i] >> 3)) {
            s_dir[i] = 0;
            continue;
        }
        hit = enemies_hit(x, s_y[i], SHOT_W, SHOT_H);
        if (hit >= 0) {
            enemies_kill(hit);
            s_dir[i] = 0;
            continue;
        }
        s_x[i] = (int16_t)x;
    }
}

int shots_pick(int *x, int *y)
{
    int k;

    for (k = 0; k < SHOT_MAX; k++) {
        const int i = (turn + k) & (SHOT_MAX - 1);

        if (s_dir[i] != 0) {
            turn = (uint8_t)(i + 1);
            *x = s_x[i];
            *y = s_y[i];
            return 1;
        }
    }
    return 0;
}
