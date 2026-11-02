/*
 * Game state machine. The debug build option START_STATE (GAME_* value, for
 * example `make shot START_STATE=1`) skips straight to a screen so it can be
 * checked in a headless snapshot.
 */
#include <stdint.h>
#include "display_list.h"
#include "enemies.h"
#include "game.h"
#include "health.h"
#include "input.h"
#include "intro.h"
#include "level.h"
#include "physics.h"
#include "screens.h"
#include "shots.h"

#ifndef START_STATE
#define START_STATE GAME_TITLE
#endif

static uint8_t state;
static uint8_t cursor_x;
static uint8_t cursor_y;
static uint16_t tick;
static unsigned prev_pad;

static void play_enter(void)
{
    physics_init();
    enemies_init();
    shots_init();
    health_init();
}

static void enter(int s)
{
    state = (uint8_t)s;
    tick = 0;
    if (s == GAME_TITLE) {
        intro_enter();
    } else if (s == GAME_SELECT) {
        cursor_x = 0;
        cursor_y = 0;
        screen_select_enter(cursor_x, cursor_y);
    } else {
        play_enter();
    }
}

void game_init(void)
{
    prev_pad = 0xFFu;               /* a button held at boot is not a press */
    enter(START_STATE);
}

/* Move the select cursor one cell, hopping over the centre (the hero's portrait). */
static void move_cursor(unsigned pressed)
{
    const int dx = ((pressed & PAD_RIGHT) ? 1 : 0) - ((pressed & PAD_LEFT) ? 1 : 0);
    const int dy = ((pressed & PAD_DOWN) ? 1 : 0) - ((pressed & PAD_UP) ? 1 : 0);
    int nx = cursor_x + dx;
    int ny = cursor_y + dy;

    if (nx == 1 && ny == 1) {
        nx += dx;
        ny += dy;
    }
    if (nx < 0 || nx > 2 || ny < 0 || ny > 2)
        return;
    cursor_x = (uint8_t)nx;
    cursor_y = (uint8_t)ny;
}

static void play_frame(unsigned pad)
{
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
    display_build();
}

#ifdef DEMO_INPUT
/*
 * Debug build (make DEMO=1): a scripted player, so a headless snapshot can show
 * the whole flow. `tick` counts frames since the current screen was entered.
 * Title: fire. Select: right, down, left, up, up, then fire on the first stage.
 */
static unsigned demo_pad(void)
{
    static const struct { uint8_t state; uint16_t at; uint8_t pad; } script[] = {
        { GAME_TITLE,  100, PAD_FIRE },     /* skip the pan */
        { GAME_TITLE,  130, PAD_FIRE },     /* leave the title */
        { GAME_SELECT,  40, PAD_RIGHT },
        { GAME_SELECT, 100, PAD_DOWN },
        { GAME_SELECT, 160, PAD_LEFT },
        { GAME_SELECT, 220, PAD_UP },
        { GAME_SELECT, 280, PAD_UP },
        { GAME_SELECT, 340, PAD_FIRE },
    };
    unsigned i;

    for (i = 0; i < sizeof script / sizeof script[0]; i++)
        if (script[i].state == state && tick >= script[i].at && tick < script[i].at + 3u)
            return script[i].pad;
    return 0;
}
#endif

void game_frame(void)
{
#ifdef DEMO_INPUT
    const unsigned pad = input_read() | demo_pad();
#else
    const unsigned pad = input_read();
#endif
    const unsigned pressed = pad & ~prev_pad;

    tick++;
    prev_pad = pad;

    if (state == GAME_TITLE) {
        if (pressed & PAD_FIRE) {
            if (intro_ready())
                enter(GAME_SELECT);
            else
                intro_skip();           /* a press during the pan jumps to the end */
        }
    } else if (state == GAME_SELECT) {
        move_cursor(pressed);
        if ((pressed & PAD_FIRE) && stage_playable(stage_at(cursor_x, cursor_y)))
            enter(GAME_PLAY);
    }

    if (state == GAME_TITLE)
        intro_frame(tick);
    else if (state == GAME_SELECT)
        screen_select_frame(cursor_x, cursor_y, tick);
    else
        play_frame(pad);
}
