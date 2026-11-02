/*
 * Frame entry point. The 6507 calls this once per frame in VBLANK (CALLFN 255);
 * it returns to the kernel when done.
 */
#include <stdint.h>
#include "audio.h"
#include "display_list.h"
#include "enemies.h"
#include "health.h"
#include "input.h"
#include "level.h"
#include "physics.h"
#include "shots.h"

/*
 * Boot code does not clear .bss on every power-on, so one-time setup is tracked
 * by a magic word at the top of cart RAM. It sits above the C stack (which starts
 * at 0x40001FDC), so nothing else uses it.
 */
#define INIT_MAGIC_ADDR ((volatile uint32_t *)0x40001FE0u)
#define INIT_MAGIC      0x5A17C0DEu

int main(void)
{
    if (*INIT_MAGIC_ADDR != INIT_MAGIC) {
        *INIT_MAGIC_ADDR = INIT_MAGIC;
        physics_init();
        enemies_init();
        shots_init();
        health_init();
        audio_init();
    }

    {
        const unsigned pad = input_read();
        int hurt = physics_step(pad);   /* a fall into a pit already respawned the hero */

        enemies_step();
        shots_step(pad);
        health_step();

        if (!health_invulnerable()) {
            /* The hero's box, inset a little so grazes don't count. */
            const int hx = physics_hero_x();
            const int idx = enemies_hit(hx + 3, physics_hero_row() + 3, HERO_W - 6, HERO_H - 6);

            if (idx >= 0) {
                int ex, ey;
                unsigned type;

                if (enemies_get(idx, &ex, &ey, &type))
                    physics_knock(hx < ex ? -1 : 1);
                hurt = 1;
            }
        }

        if (hurt && health_hurt()) {
            /* Out of health: start over, with every enemy back in place. */
            physics_respawn();
            enemies_init();
            shots_init();
            health_init();
        }
    }
    audio_update();
    display_build();
    return 0;
}
