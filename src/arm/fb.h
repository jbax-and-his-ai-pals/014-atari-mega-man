/*
 * Framebuffer screens (title, stage select): a 1-bit bitmap of 40 cells x 192
 * lines, one cell being a 4 px playfield block. Unlike the tile map it has
 * one-line vertical resolution, so text and logos can be detailed.
 *
 * Bit c of a line's 40-bit row is cell column c (LSB first, like level_bits).
 * Cell columns 0-1 fall under the kernel's HMOVE bar and are never visible.
 * The framebuffer lives in display data (DD_FB) and persists between frames,
 * so a static screen is drawn once and only the changing parts are redrawn.
 */
#ifndef FB_H
#define FB_H

#include <stdint.h>

#define FB_COLS   40
#define FB_LINES  192

/* Bit-reverse of every byte value (framebuffer cells and sprite rows read opposite ways). */
extern const uint8_t fb_rev8[256];

void fb_clear(void);

/* Set (or clear) the rectangle of cells col.., line.., w cells wide, h lines tall. */
void fb_fill(int col, int line, int w, int h, int set);

/* OR a row of 8 cells into h lines at (col, line); bit 7 of `row` is the leftmost. */
void fb_row8(int col, int line, int h, unsigned row);

/*
 * Text in a 5-row font (A-Z, 0-9, space - . ! ?). Letters are 3 pixels wide,
 * except M, N and W which are 5. Each font pixel is sx cells wide and sy lines
 * tall, with a one-pixel gap between characters.
 */
void fb_text(int col, int line, const char *s, int sx, int sy);
int fb_text_width(const char *s, int sx);

/* Write every line of the framebuffer into the playfield streams. */
void fb_present(void);

/* Pack 20 cells (bit 0 leftmost) into PF0, PF1, PF2. */
void fb_pack_half(uint32_t bits, uint8_t out[3]);

#endif /* FB_H */
