/*
 * Framebuffer screens: drawing primitives and the packer that turns the bitmap
 * into per-line PF0/PF1/PF2 bytes. No division, floats or libc.
 */
#include <stdint.h>
#include "cdfj.h"
#include "fb.h"

#define FB_STRIDE 5                 /* bytes per line: 40 cells */
#define FB_BYTES  (FB_LINES * FB_STRIDE)

#define FB ((volatile uint8_t *)(DD_BASE_ARM + DD_FB))
#define FBW ((volatile uint32_t *)(DD_BASE_ARM + DD_FB))

/* Bit-reverse a byte: PF1 shows its bits D7 first, the framebuffer stores leftmost first. */
#define R2(n) n, n + 2 * 64, n + 1 * 64, n + 3 * 64
#define R4(n) R2(n), R2(n + 2 * 16), R2(n + 1 * 16), R2(n + 3 * 16)
#define R6(n) R4(n), R4(n + 2 * 4), R4(n + 1 * 4), R4(n + 3 * 4)
const uint8_t fb_rev8[256] = { R6(0), R6(2), R6(1), R6(3) };

/* 3x5 font; each row is 3 bits with bit 2 the left column. */
static const uint8_t font[][5] = {
    {2,5,7,5,5}, {6,5,7,5,7}, {3,4,4,4,3}, {6,5,5,5,6}, {7,4,6,4,7},   /* A-E */
    {7,4,6,4,4}, {3,4,5,5,3}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,2},   /* F-J */
    {5,5,6,5,5}, {4,4,4,4,7}, {5,7,7,5,5}, {6,5,5,5,5}, {2,5,5,5,2},   /* K-O */
    {6,5,6,4,4}, {2,5,5,6,3}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2},   /* P-T */
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,2,2,2},   /* U-Y */
    {7,1,2,4,7},                                                        /* Z */
    {7,5,5,5,7}, {2,6,2,2,7}, {6,1,2,4,7}, {6,1,2,1,6}, {5,5,7,1,1},   /* 0-4 */
    {7,4,6,1,6}, {3,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,6},   /* 5-9 */
    {0,0,0,0,0}, {0,0,7,0,0}, {0,0,0,0,2}, {2,2,2,0,2}, {6,1,2,0,2},   /* space - . ! ? */
};

static int glyph_index(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '0' && c <= '9')
        return 26 + (c - '0');
    if (c == '-')
        return 37;
    if (c == '.')
        return 38;
    if (c == '!')
        return 39;
    if (c == '?')
        return 40;
    return 36;
}

void fb_clear(void)
{
    unsigned i;

    for (i = 0; i < FB_BYTES / 4; i++)
        FBW[i] = 0;
}

void fb_fill(int col, int line, int w, int h, int set)
{
    int y, last, b;

    if (col < 0) {
        w += col;
        col = 0;
    }
    if (col + w > FB_COLS)
        w = FB_COLS - col;
    if (line < 0) {
        h += line;
        line = 0;
    }
    if (line + h > FB_LINES)
        h = FB_LINES - line;
    if (w <= 0 || h <= 0)
        return;

    last = col + w - 1;
    for (y = line; y < line + h; y++) {
        for (b = col >> 3; b <= (last >> 3); b++) {
            unsigned mask = 0xFFu;

            if (b == (col >> 3))
                mask &= 0xFFu << (col & 7);
            if (b == (last >> 3))
                mask &= 0xFFu >> (7 - (last & 7));
            if (set)
                FB[y * FB_STRIDE + b] |= (uint8_t)mask;
            else
                FB[y * FB_STRIDE + b] &= (uint8_t)~mask;
        }
    }
}

void fb_row8(int col, int line, int h, unsigned row)
{
    const unsigned bits = (unsigned)fb_rev8[row & 0xFFu] << (col & 7);
    const int b = col >> 3;
    int y;

    for (y = line; y < line + h; y++) {
        if (y < 0 || y >= FB_LINES)
            continue;
        FB[y * FB_STRIDE + b] |= (uint8_t)bits;
        if ((bits >> 8) && b + 1 < FB_STRIDE)
            FB[y * FB_STRIDE + b + 1] |= (uint8_t)(bits >> 8);
    }
}

/*
 * M, N and W need five columns to read as themselves: at three they turn into
 * H. Their rows are 5 bits, bit 4 leftmost.
 */
static const uint8_t wide_font[3][5] = {
    {0x11, 0x1B, 0x15, 0x11, 0x11},     /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11},     /* N */
    {0x11, 0x11, 0x15, 0x1B, 0x11},     /* W */
};

static int wide_index(char c)
{
    return c == 'M' ? 0 : c == 'N' ? 1 : c == 'W' ? 2 : -1;
}

int fb_text_width_ex(const char *s, int sx, int wide)
{
    int w = 0;

    for (; *s; s++)
        w += ((wide && wide_index(*s) >= 0 ? 5 : 3) + 1) * sx;
    return w ? w - sx : 0;
}

int fb_text_width(const char *s, int sx)
{
    return fb_text_width_ex(s, sx, 1);
}

void fb_text(int col, int line, const char *s, int sx, int sy)
{
    fb_text_ex(col, line, s, sx, sy, 1);
}

void fb_text_ex(int col, int line, const char *s, int sx, int sy, int wide)
{
    for (; *s; s++) {
        const int wi = wide ? wide_index(*s) : -1;
        const int gw = wi >= 0 ? 5 : 3;
        const uint8_t *g = wi >= 0 ? wide_font[wi] : font[glyph_index(*s)];
        int r, c;

        for (r = 0; r < 5; r++)
            for (c = 0; c < gw; c++)
                if ((g[r] >> (gw - 1 - c)) & 1)
                    fb_fill(col + c * sx, line + r * sy, sx, sy, 1);
        col += (gw + 1) * sx;
    }
}

void fb_pack_half(uint32_t bits, uint8_t out[3])
{
    out[0] = (uint8_t)((bits & 0x0Fu) << 4);
    out[1] = fb_rev8[(bits >> 4) & 0xFFu];
    out[2] = (uint8_t)((bits >> 12) & 0xFFu);
}

void fb_line(unsigned line, uint32_t *left, uint32_t *right)
{
    const volatile uint8_t *p = &FB[line * FB_STRIDE];
    const uint32_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3], b4 = p[4];

    *left = b0 | (b1 << 8) | ((b2 & 0x0Fu) << 16);
    *right = (b2 >> 4) | (b3 << 4) | (b4 << 12);
}

void fb_present(void)
{
    unsigned line;

    for (line = 0; line < FB_LINES; line++) {
        const volatile uint8_t *p = &FB[line * FB_STRIDE];
        const uint32_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3], b4 = p[4];
        uint8_t left[3], right[3];

        fb_pack_half(b0 | (b1 << 8) | ((b2 & 0x0Fu) << 16), left);
        fb_pack_half((b2 >> 4) | (b3 << 4) | (b4 << 12), right);
        CDF_DD[DD_PF0L + line] = left[0];
        CDF_DD[DD_PF1L + line] = left[1];
        CDF_DD[DD_PF2L + line] = left[2];
        CDF_DD[DD_PF0R + line] = right[0];
        CDF_DD[DD_PF1R + line] = right[1];
        CDF_DD[DD_PF2R + line] = right[2];
    }
}
