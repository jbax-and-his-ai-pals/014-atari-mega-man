/*
 * Hit points and the invulnerability window after a hit.
 *
 * Every static is assigned in health_init(): RAM is not zeroed at boot.
 */
#include <stdint.h>
#include "health.h"

#define INVULN_FRAMES 90            /* 1.5 s */

static uint8_t hp;
static uint8_t invuln;

void health_init(void)
{
    hp = HEALTH_MAX;
    invuln = 0;
}

void health_step(void)
{
    if (invuln)
        invuln--;
}

int health_invulnerable(void)
{
    return invuln != 0;
}

int health_blink(void)
{
    return invuln != 0 && (invuln & 4u) != 0;
}

int health_hurt(void)
{
    if (hp > 0)
        hp--;
    invuln = INVULN_FRAMES;
    return hp == 0;
}

int health_points(void)
{
    return hp;
}
