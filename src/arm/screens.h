/*
 * The stage-select screen. It is drawn into the
 * framebuffer (fb.h): *_enter() draws the static art once, *_frame() redraws
 * only what changes and sets up the display for the frame.
 */
#ifndef SCREENS_H
#define SCREENS_H

#define STAGE_COUNT 8

/* Full name ("SAW MAN") and the 3-letter label shown under the portrait ("SAW"). */
const char *stage_name(int stage);
const char *stage_short(int stage);

/* Stages that have a level built so far. */
int stage_playable(int stage);

/* Stage in grid cell (gx, gy) of the 3x3 select grid, or -1 for the centre cell. */
int stage_at(int gx, int gy);

void screen_select_enter(int gx, int gy);
void screen_select_frame(int gx, int gy, unsigned tick);

#endif /* SCREENS_H */
