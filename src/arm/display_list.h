#ifndef DISPLAY_LIST_H
#define DISPLAY_LIST_H

#include <stdint.h>

/* Fill the per-scanline tables for the play screen and reset the datastream
 * pointers. VBLANK only. */
void display_build(void);

/*
 * Pieces for the framebuffer screens (title, stage select). Each frame: draw the
 * framebuffer, fb_present(), then display_sky(), one of display_no_sprites() or
 * display_one_sprite(), and finally display_finish().
 */

/* Background gradient: hue is the high nibble of the color, lum0 the top line's
 * luminance, and the luminance rises by one every 1 << shift lines. */
void display_sky(uint8_t hue, unsigned lum0, unsigned shift);

/* Write one scanline's playfield: 20 cells per half, bit 0 of each leftmost. */
void display_pf_line(unsigned line, uint32_t left, uint32_t right);

/* Background color for one scanline (0..LINES, one past the last is read in VBLANK). */
void display_bk_line(unsigned line, uint8_t color);

typedef struct {
    int16_t x;                      /* screen X, 0-143 */
    int16_t top;                    /* top line, may be negative */
    uint8_t frame;                  /* ART_* frame from art_data.h */
    uint8_t flip;                   /* 1 = mirror */
} display_sprite_t;

/*
 * Several sprites, sorted by top line. They share the one sprite pair, so ones
 * too close to drift between take turns. Pass only sprites that are on screen.
 */
void display_sprites(const display_sprite_t *s, int n);

/* No sprites or bullet this frame. */
void display_no_sprites(void);

/* One 16x16 sprite (an ART_* frame from art_data.h, mirrored if flip) at screen X
 * (0-143) and top line, which may be negative. */
void display_one_sprite(int x, int top, int frame, int flip);

/* Playfield color, and point the streams at their tables. Call last. */
void display_finish(uint8_t colupf);

/*
 * Scene mode: the 6507 runs its scene kernel, which takes the playfield color
 * from a per-line table instead of one color for the frame. Sprites here must
 * start at x >= 62, and drift and bullets are not available. Write each line with
 * display_pf_line / display_bk_line / display_pfcolor_line, then call
 * display_finish_scene() last instead of display_finish().
 */
void display_pfcolor_line(unsigned line, uint8_t color);
void display_finish_scene(void);

#endif /* DISPLAY_LIST_H */
