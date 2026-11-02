#ifndef DISPLAY_LIST_H
#define DISPLAY_LIST_H

#include <stdint.h>

/* Sprite art, indexed into the table in display_list.c. */
enum { ART_HERO, ART_WALKER, ART_FLYER };

/* Fill the per-scanline tables for the play screen and reset the datastream
 * pointers. VBLANK only. */
void display_build(void);

/*
 * Pieces for the framebuffer screens (title, stage select). Each frame: draw the
 * framebuffer, fb_present(), then display_sky(), one of display_no_sprites() or
 * display_one_sprite(), and finally display_finish().
 */

/* Background gradient: hue is the high nibble of the color, lum0 the top line's luminance. */
void display_sky(uint8_t hue, unsigned lum0);

/* No sprites or bullet this frame. */
void display_no_sprites(void);

/* One 16x16 sprite at screen X (0-143) and top line, which may be negative. */
void display_one_sprite(int x, int top, int art_id);

/* Playfield color, and point the streams at their tables. Call last. */
void display_finish(uint8_t colupf);

#endif /* DISPLAY_LIST_H */
