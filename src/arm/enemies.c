/*
 * Enemy behaviour. 8.8 fixed point, no division, and every static is assigned
 * in enemies_init() (RAM is not guaranteed zeroed or initialised at boot).
 */
#include <stdint.h>
#include "level.h"
#include "enemies.h"

#define MAX_ENEMIES   16
#define WALK_SPEED    128      /* 0.5 px/frame in 8.8 */
#define FLY_SPEED     192      /* 0.75 px/frame in 8.8 */
#define FLY_RANGE     32       /* patrol +/- px around spawn x */
#define BOB_AMP       6

/* Quarter wave of the vertical bob, 0..BOB_AMP, 16 samples. */
static const uint8_t bob_q[16] = { 0, 1, 1, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6 };

static int32_t e_x[MAX_ENEMIES];      /* 8.8 left edge */
static int32_t e_xlo[MAX_ENEMIES];    /* 8.8 patrol limits (flyers) */
static int32_t e_xhi[MAX_ENEMIES];
static int16_t e_y[MAX_ENEMIES];      /* current top, px */
static int16_t e_y0[MAX_ENEMIES];     /* spawn top, px */
static int8_t  e_dir[MAX_ENEMIES];    /* -1 / +1 */
static uint8_t e_type[MAX_ENEMIES];
static uint8_t e_alive[MAX_ENEMIES];
static uint8_t e_phase[MAX_ENEMIES];  /* 0..63 bob phase */
static uint8_t e_n;

static int bob_offset(unsigned phase)
{
    unsigned q = phase & 15u;

    switch ((phase >> 4) & 3u) {
    case 0:  return (int)bob_q[q];
    case 1:  return (int)bob_q[15u - q];
    case 2:  return -(int)bob_q[q];
    default: return -(int)bob_q[15u - q];
    }
}

/* No libc on the cart: keep gcc from turning the clearing loop into memset(). */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void enemies_init(void)
{
    int i;
    int n = (int)LEVEL_NUM_ENEMIES;

    if (n > MAX_ENEMIES)
        n = MAX_ENEMIES;
    e_n = (uint8_t)n;
    for (i = 0; i < MAX_ENEMIES; i++) {
        e_x[i] = 0;
        e_xlo[i] = 0;
        e_xhi[i] = 0;
        e_y[i] = 0;
        e_y0[i] = 0;
        e_dir[i] = -1;
        e_type[i] = 0;
        e_alive[i] = 0;
        e_phase[i] = 0;
    }
    for (i = 0; i < n; i++) {
        int x = level_enemies[i].x;

        e_x[i] = (int32_t)x << 8;
        e_y[i] = level_enemies[i].y;
        e_y0[i] = level_enemies[i].y;
        e_type[i] = level_enemies[i].type;
        e_alive[i] = 1;
        e_dir[i] = (LEVEL_SPAWN_X < x) ? -1 : 1;
        e_xlo[i] = (int32_t)(x - FLY_RANGE) << 8;
        e_xhi[i] = (int32_t)(x + FLY_RANGE) << 8;
        e_phase[i] = (uint8_t)((i * 13) & 63);
    }
}

/* Is any tile in rows [y0, y1] (px lines) at pixel column px solid? */
static int column_blocked(int px, int y0, int y1)
{
    int col = px >> 2;
    int row;
    int r1 = y1 >> 3;

    for (row = y0 >> 3; row <= r1; row++)
        if (level_solid(col, row))
            return 1;
    return 0;
}

static void step_walker(int i)
{
    int32_t nx = e_x[i] + (e_dir[i] > 0 ? WALK_SPEED : -WALK_SPEED);
    int px = (int)(nx >> 8);
    int lead = (e_dir[i] > 0) ? px + ENEMY_W - 1 : px;
    int y = e_y[i];

    if (column_blocked(lead, y, y + ENEMY_H - 1)
        || !level_solid(lead >> 2, (y + ENEMY_H) >> 3)) {
        e_dir[i] = (int8_t)-e_dir[i];
    } else {
        e_x[i] = nx;
    }
}

static void step_flyer(int i)
{
    int32_t nx = e_x[i] + (e_dir[i] > 0 ? FLY_SPEED : -FLY_SPEED);
    int px = (int)(nx >> 8);
    int lead = (e_dir[i] > 0) ? px + ENEMY_W - 1 : px;
    int y0 = e_y0[i];

    e_phase[i] = (uint8_t)((e_phase[i] + 1u) & 63u);
    e_y[i] = (int16_t)(y0 + bob_offset(e_phase[i]));

    if (nx < e_xlo[i] || nx > e_xhi[i]
        || column_blocked(lead, y0 - BOB_AMP, y0 + ENEMY_H - 1 + BOB_AMP)) {
        e_dir[i] = (int8_t)-e_dir[i];
    } else {
        e_x[i] = nx;
    }
}

void enemies_step(void)
{
    int i;

    for (i = 0; i < (int)e_n; i++) {
        if (!e_alive[i])
            continue;
        if (e_type[i] == ENEMY_FLYER)
            step_flyer(i);
        else
            step_walker(i);
    }
}

int enemies_count(void)
{
    return (int)e_n;
}

int enemies_get(int i, int *x, int *y, unsigned *type)
{
    if (i < 0 || i >= (int)e_n || !e_alive[i])
        return 0;
    *x = (int)(e_x[i] >> 8);
    *y = (int)e_y[i];
    *type = e_type[i];
    return 1;
}

int enemies_touch(int hx, int hy, int hw, int hh)
{
    int i;
    int l = hx + 2, r = hx + hw - 2;
    int t = hy + 2, b = hy + hh - 2;

    for (i = 0; i < (int)e_n; i++) {
        int ex, ey;

        if (!e_alive[i])
            continue;
        ex = (int)(e_x[i] >> 8) + 2;
        ey = (int)e_y[i] + 2;
        if (l < ex + ENEMY_W - 4 && r > ex && t < ey + ENEMY_H - 4 && b > ey)
            return 1;
    }
    return 0;
}
