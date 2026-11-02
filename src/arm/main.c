/*
 * Frame entry point. The 6507 calls this once per frame in VBLANK (CALLFN 255);
 * it returns to the kernel when done.
 */
#include <stdint.h>
#include "audio.h"
#include "display_list.h"
#include "enemies.h"
#include "input.h"
#include "level.h"
#include "physics.h"

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
        audio_init();
    }

    physics_step(input_read());
    enemies_step();
    if (enemies_touch(physics_hero_x(), physics_hero_row(), HERO_W, HERO_H))
        physics_respawn();
    audio_update();
    display_build();
    return 0;
}
