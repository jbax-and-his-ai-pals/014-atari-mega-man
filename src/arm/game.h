/*
 * The game's top-level state machine: title -> stage select -> play.
 */
#ifndef GAME_H
#define GAME_H

enum { GAME_TITLE, GAME_SELECT, GAME_PLAY };

/* Set up the state machine. Call once at boot; starts at START_STATE (default title). */
void game_init(void);

/* Run one frame: read the controls, update, and build the display. */
void game_frame(void);

#endif /* GAME_H */
