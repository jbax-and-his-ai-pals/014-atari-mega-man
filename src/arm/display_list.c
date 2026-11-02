/*
 * Builds the per-scanline tables the 6507 kernel streams in.
 *
 * Playfield: a 40-column window into the tile map (level.h), one tile row per
 * 8 scanlines, over a sky gradient. The playfield is asymmetric: the left and
 * right 20 bits are stored separately.
 *
 * Camera: follows the hero. The playfield can only move in whole 4 px columns,
 * so the camera snaps to a column; sprites still move one pixel at a time.
 *
 * Sprites: every object (the hero and each enemy) is 8 pixels wide and 16 tall,
 * so each needs only one player sprite. The two players are independent channels:
 * each streams its own graphics, color and drift (the kernel does HMOVE on every
 * line), so a channel drifts from one object's X to the next one's during the
 * lines between them (up to 8 px right or 7 px left per line). Objects are dealt
 * onto the two channels; ones too close to share a channel, with no free one,
 * take turns (the one skipped longest is drawn), so they flicker instead of
 * vanishing. Bullets are playfield cells, so they are free.
 *
 * The whole table is rebuilt every frame, so nothing here depends on state
 * left over from boot.
 */
#include <stdint.h>
#include "art_data.h"
#include "cdfj.h"
#include "display_list.h"
#include "enemies.h"
#include "fb.h"
#include "health.h"
#include "input.h"
#include "level.h"
#include "physics.h"
#include "shots.h"

/* Stream n -> display-data table. Order matches STREAM_* in layout.inc. */
static const uint16_t stream_base[NUM_STREAMS] = {
    DD_PF0L, DD_PF1L, DD_PF2L, DD_PF0R, DD_PF1R, DD_PF2R,
    DD_GRP0, DD_GRP1, DD_COLUP0, DD_COLUP1, DD_HM0, DD_HM1, DD_BK,
    DD_COLUPF, DD_TOPX0, DD_TOPX1, DD_COLUPFL, DD_MODE
};

#define SCREEN_COLS   40
#define SCREEN_W      (SCREEN_COLS * TILE_W)
#define CAMERA_MAX    (LEVEL_W_PX - SCREEN_W)
#define HERO_SCREEN_X 72            /* where the camera keeps the hero, mid-level */
#define OBJ_W         8
#define OBJ_H         16
#define OBJ_X_MAX     (SCREEN_W - OBJ_W)
#define MAX_OBJS      (1 + 16)      /* the hero, then up to 16 enemies */

/* PosObject in kernel.asm places a sprite this many pixels left of the value
 * it is given; measured with tools/snap.py. */
#define HERO_X_TRIM 3

/* P1's graphics land at clock 84 (see kernel.asm), so a P1 object left of this
 * would show the previous row's shape; those go on P0. */
#define P1_MIN_X    16

/* Hero frames, indexed by physics_hero_pose(). */
static const uint8_t hero_frames[5] = {
    ART_HERO_IDLE, ART_HERO_RUN_A, ART_HERO_RUN_P, ART_HERO_RUN_B, ART_HERO_JUMP
};

/* Advances once per frame; drives the enemies' walk and flap cycles. */
static uint8_t anim;

typedef struct {
    int16_t x;                      /* screen X of the left edge, 0..OBJ_X_MAX */
    int16_t top;                    /* first scanline, may be negative */
    uint8_t frame;                  /* ART_* frame index */
    uint8_t flip;                   /* 1 = mirror (the art faces right) */
    uint8_t id;                     /* index into starve[] */
} obj_t;

/*
 * Frames since each object (0 = hero, 1+i = enemy i) was last drawn. Starts as
 * whatever is in RAM; it only biases who wins a conflict, so that is harmless.
 */
static uint8_t starve[MAX_OBJS];

static void dd_put(unsigned off, uint8_t v)
{
    CDF_DD[off] = v;
}

/* Left pixel of the screen in the level, snapped to a whole column. */
static int camera_x(int hero_x)
{
    int cam = hero_x - HERO_SCREEN_X;

    if (cam < 0)
        cam = 0;
    if (cam > CAMERA_MAX)
        cam = CAMERA_MAX;
    return cam & ~(TILE_W - 1);
}

/* Lines of HMOVE needed to drift dx pixels (8 per line right, 7 left). */
static int drift_lines(int dx)
{
    int n = 0;

    while (dx > 0) {
        dx -= 8;
        n++;
    }
    while (dx < 0) {
        dx += 7;
        n++;
    }
    return n;
}

/* Can `next` be drawn after `prev` on the same sprite channel? */
static int fits_after(const obj_t *prev, const obj_t *next)
{
    const int first_free = prev->top + OBJ_H;          /* first line after prev */
    const int gap = next->top - first_free;            /* blank lines between */
    const int start = first_free > 0 ? first_free : 0;

    /* No overlap, and enough HMOVE lines (the gap plus next's first line, whose
     * HMOVE runs before it is drawn) to drift over. Colors switch per line, so
     * objects may touch. */
    return gap >= 0 && next->top - start + 1 >= drift_lines(next->x - prev->x);
}

/* Collect the objects on screen, sorted by top line. Returns the count. */
static int gather(obj_t *objs, int cam)
{
    int n = 0, i, k;
    const int ne = enemies_count();

    if (!health_blink()) {          /* hidden on alternate frames while invulnerable */
        objs[n].x = (int16_t)(physics_hero_x() - cam);
        objs[n].top = (int16_t)physics_hero_row();
        objs[n].frame = hero_frames[physics_hero_pose()];
        objs[n].flip = physics_hero_facing() < 0;
        objs[n].id = 0;
        n++;
    }

    for (i = 0; i < ne && n < MAX_OBJS; i++) {
        int ex, ey;
        unsigned type;

        if (!enemies_get(i, &ex, &ey, &type))
            continue;
        ex -= cam;
        if (ex < 0 || ex > OBJ_X_MAX || ey <= -OBJ_H || ey >= (int)LINES)
            continue;
        objs[n].x = (int16_t)ex;
        objs[n].top = (int16_t)ey;
        {
            const int alt = (int)((anim >> 3) + i) & 1;     /* each enemy at its own phase */

            if (type == ENEMY_FLYER)
                objs[n].frame = alt ? ART_FLYER_DOWN : ART_FLYER_UP;
            else
                objs[n].frame = alt ? ART_WALKER_B : ART_WALKER_A;
        }
        objs[n].flip = enemies_dir(i) < 0;
        objs[n].id = (uint8_t)(1 + i);
        n++;
    }

    /* Insertion sort by top line; n is at most 17. */
    for (i = 1; i < n; i++) {
        obj_t o = objs[i];

        for (k = i; k > 0 && objs[k - 1].top > o.top; k--)
            objs[k] = objs[k - 1];
        objs[k] = o;
    }
    return n;
}

#define CHANNELS    2
#define MAX_PER_CH  12              /* 192 lines / 16 */

typedef struct {
    obj_t o[MAX_PER_CH];            /* in top-to-bottom order */
    int n;
} chan_t;

static int chan_allows(int c, const obj_t *o)
{
    return c == 0 || o->x >= P1_MIN_X;
}

/* Can o follow channel c's objects, ignoring its last `skip` ones? */
static int chan_fits(const chan_t *ch, int skip, const obj_t *o)
{
    const int n = ch->n - skip;

    return n <= 0 || fits_after(&ch->o[n - 1], o);
}

static int chan_end(const chan_t *ch)
{
    return ch->n ? ch->o[ch->n - 1].top + OBJ_H : -1000;
}

/*
 * Deal the objects (sorted by top line) onto the two channels. An object goes
 * where it fits without dropping anything, preferring the channel whose last
 * object ends closest above it; the hero prefers P0. When neither channel has
 * room, the object takes the place of a channel's last one if it has waited
 * longer (the hero never loses its place). Updates starve[].
 */
static void schedule(const obj_t *objs, int n, chan_t *ch)
{
    int i, c, k;

    ch[0].n = 0;
    ch[1].n = 0;
    for (i = 0; i < n; i++)
        if (starve[objs[i].id] < 255)
            starve[objs[i].id]++;

    for (i = 0; i < n; i++) {
        const obj_t *o = &objs[i];
        int best = -1;

        for (c = 0; c < CHANNELS; c++) {
            if (!chan_allows(c, o) || !chan_fits(&ch[c], 0, o) || ch[c].n >= MAX_PER_CH)
                continue;
            if (best < 0 || (o->id != 0 && chan_end(&ch[c]) > chan_end(&ch[best])))
                best = c;
        }
        if (best >= 0) {
            ch[best].o[ch[best].n++] = *o;
            continue;
        }

        for (c = 0; c < CHANNELS; c++) {
            const obj_t *last;

            if (!chan_allows(c, o) || ch[c].n == 0)
                continue;
            last = &ch[c].o[ch[c].n - 1];
            if (last->id == 0 || !chan_fits(&ch[c], 1, o))
                continue;
            if (o->id != 0 && starve[o->id] <= starve[last->id])
                continue;
            if (best < 0 || starve[last->id] < starve[ch[best].o[ch[best].n - 1].id])
                best = c;
        }
        if (best >= 0)
            ch[best].o[ch[best].n - 1] = *o;
    }

    for (c = 0; c < CHANNELS; c++)
        for (k = 0; k < ch[c].n; k++)
            starve[ch[c].o[k].id] = 0;
}

static uint8_t hm_byte(int move)    /* move > 0: right, < 0: left */
{
    if (move > 0)
        return (uint8_t)(((16 - move) & 15) << 4);
    return (uint8_t)((-move) << 4);
}

static void build_sprites(const chan_t *ch)
{
    static const uint16_t grp_base[CHANNELS] = { DD_GRP0, DD_GRP1 };
    static const uint16_t col_base[CHANNELS] = { DD_COLUP0, DD_COLUP1 };
    static const uint16_t hm_base[CHANNELS] = { DD_HM0, DD_HM1 };
    int c, i, k;
    unsigned line;

    for (line = 0; line < LINES; line++) {
        dd_put(DD_GRP0 + line, 0);
        dd_put(DD_GRP1 + line, 0);
    }
    for (line = 0; line <= LINES; line++) {
        dd_put(DD_COLUP0 + line, 0);
        dd_put(DD_COLUP1 + line, 0);
        dd_put(DD_HM0 + line, 0);
        dd_put(DD_HM1 + line, 0);
    }

    for (c = 0; c < CHANNELS; c++) {
        const obj_t *s = ch[c].o;
        const int m = ch[c].n;

        for (i = 0; i < m; i++) {
            const art_frame_t *f = &art_frames[s[i].frame];

            /* Graphics and color for each row that is on screen: the left 8
             * pixels of the frame. Each row has its own color. */
            for (k = 0; k < OBJ_H; k++) {
                const int y = s[i].top + k;

                if (y >= 0 && y < (int)LINES) {
                    uint8_t row = (uint8_t)(f->rows[k] >> 8);

                    if (s[i].flip)
                        row = fb_rev8[row];
                    dd_put(grp_base[c] + y, row);
                    dd_put(col_base[c] + y, f->colors[k]);
                }
            }

            /* Drift to the next object over the lines after this one. */
            if (i + 1 < m) {
                int dx = s[i + 1].x - s[i].x;

                k = s[i].top + OBJ_H;
                if (k < 0)
                    k = 0;
                while (dx != 0 && k <= (int)LINES) {
                    int move = dx > 8 ? 8 : (dx < -7 ? -7 : dx);

                    dd_put(hm_base[c] + k, hm_byte(move));
                    dx -= move;
                    k++;
                }
            }
        }
    }

    dd_put(DD_TOPX0, (uint8_t)((ch[0].n > 0 ? ch[0].o[0].x : 0) + HERO_X_TRIM));
    dd_put(DD_TOPX1, (uint8_t)((ch[1].n > 0 ? ch[1].o[0].x : 0) + HERO_X_TRIM));
}

/* One 2-tile pip per health point, in the left 20 playfield columns. */
static uint32_t hud_bits(void)
{
    uint32_t bits = 0;
    int i;

    for (i = 0; i < health_points(); i++)
        bits |= 3u << (3 + 3 * i);
    return bits;
}

/* Set playfield cell `cell` (0-39) on scanline `line`, over whatever is there. */
static void pf_set_cell(int line, int cell)
{
    static const uint16_t left_base[3] = { DD_PF0L, DD_PF1L, DD_PF2L };
    static const uint16_t right_base[3] = { DD_PF0R, DD_PF1R, DD_PF2R };
    const uint16_t *base = cell < 20 ? left_base : right_base;
    int reg, bit;

    if (cell >= 20)
        cell -= 20;
    if (cell < 4) {                         /* PF0: D4..D7, leftmost first */
        reg = 0;
        bit = 4 + cell;
    } else if (cell < 12) {                 /* PF1: D7..D0 */
        reg = 1;
        bit = 7 - (cell - 4);
    } else {                                /* PF2: D0..D7 */
        reg = 2;
        bit = cell - 12;
    }
    CDF_DD[base[reg] + line] |= (uint8_t)(1u << bit);
}

/* Bullets are playfield cells: 4 px wide like a cell, and they move a cell a frame. */
static void draw_shots(int cam)
{
    int i, sx, sy, k;

    for (i = 0; shots_get(i, &sx, &sy); i++) {
        const int cell = (sx - cam) >> 2;

        if (cell < 0 || cell >= SCREEN_COLS)
            continue;
        for (k = 0; k < SHOT_H; k++)
            if (sy + k >= 0 && sy + k < (int)LINES)
                pf_set_cell(sy + k, cell);
    }
}

#ifdef PERF_METER
/*
 * Debug bar (make PERF=1) at the top right: how much of VBLANK the last frame's
 * ARM work left free. Full is 16 cells; empty means the frame overran.
 */
static void perf_meter(void)
{
    unsigned cells = (input_headroom() * 3u) >> 3;      /* 0..43 -> 0..16 */
    unsigned line;

    if (cells > 16u)
        cells = 16u;
    for (line = 2; line < 6; line++) {
        uint8_t left[3], right[3];

        fb_pack_half(0, left);
        fb_pack_half(((1u << cells) - 1u) << 2, right);     /* cells 22.. */
        dd_put(DD_PF0R + line, right[0]);
        dd_put(DD_PF1R + line, right[1]);
        dd_put(DD_PF2R + line, right[2]);
        (void)left;
    }
}
#endif

void display_build(void)
{
    const int cam = camera_x(physics_hero_x());
    const unsigned col0 = (unsigned)cam / TILE_W;
    obj_t objs[MAX_OBJS];
    chan_t ch[CHANNELS];
    unsigned row, line = 0;

    anim++;

    for (row = 0; row < LEVEL_ROWS; row++) {
        uint8_t left[3], right[3];
        unsigned k;

        if (row == 0) {
            /* Health pips replace the (empty) top tile row: 2 tiles each, 1 apart,
             * starting at column 3 so they clear the HMOVE bar. */
            fb_pack_half(hud_bits(), left);
            fb_pack_half(0, right);
        } else {
            fb_pack_half(level_row_bits(row, col0), left);
            fb_pack_half(level_row_bits(row, col0 + 20), right);
        }

        for (k = 0; k < TILE_H; k++, line++) {
            dd_put(DD_PF0L + line, left[0]);
            dd_put(DD_PF1L + line, left[1]);
            dd_put(DD_PF2L + line, left[2]);
            dd_put(DD_PF0R + line, right[0]);
            dd_put(DD_PF1R + line, right[1]);
            dd_put(DD_PF2R + line, right[2]);
        }
    }

    display_sky(0x80u, 0, 6);       /* black, fading to deep blue toward the floor */
#ifdef PERF_METER
    perf_meter();
#endif

    schedule(objs, gather(objs, cam), ch);
    build_sprites(ch);
    draw_shots(cam);

    display_finish(0x1Eu);          /* platform color: gold */
}

void display_sky(uint8_t hue, unsigned lum0, unsigned shift)
{
    unsigned line;

    for (line = 0; line <= LINES; line++) {
        unsigned lum = lum0 + (line >> shift);

        if (lum > 7u)
            lum = 7u;
        dd_put(DD_BK + line, (uint8_t)(hue | (lum << 1)));
    }
}

void display_pf_line(unsigned line, uint32_t left, uint32_t right)
{
    uint8_t l[3], r[3];

    fb_pack_half(left, l);
    fb_pack_half(right, r);
    dd_put(DD_PF0L + line, l[0]);
    dd_put(DD_PF1L + line, l[1]);
    dd_put(DD_PF2L + line, l[2]);
    dd_put(DD_PF0R + line, r[0]);
    dd_put(DD_PF1R + line, r[1]);
    dd_put(DD_PF2R + line, r[2]);
}

void display_bk_line(unsigned line, uint8_t color)
{
    dd_put(DD_BK + line, color);
}

void display_sprites(const display_sprite_t *s, int n)
{
    obj_t objs[MAX_OBJS];
    chan_t ch[CHANNELS];
    int i;

    if (n > MAX_OBJS)
        n = MAX_OBJS;
    for (i = 0; i < n; i++) {
        objs[i].x = s[i].x;
        objs[i].top = s[i].top;
        objs[i].frame = s[i].frame;
        objs[i].flip = s[i].flip;
        objs[i].id = (uint8_t)i;
    }
    schedule(objs, n, ch);
    build_sprites(ch);
}

void display_no_sprites(void)
{
    chan_t ch[CHANNELS];

    ch[0].n = 0;
    ch[1].n = 0;
    build_sprites(ch);
}

void display_one_sprite(int x, int top, int frame, int flip)
{
    display_sprite_t s;

    s.x = (int16_t)x;
    s.top = (int16_t)top;
    s.frame = (uint8_t)frame;
    s.flip = (uint8_t)flip;
    display_sprites(&s, 1);
}

/* Point each stream at its table, and step one byte per fetch. */
static void finish(uint8_t colupf, uint8_t scene)
{
    unsigned n;

    dd_put(DD_COLUPF, colupf);
    dd_put(DD_MODE, scene);

    for (n = 0; n < NUM_STREAMS; n++) {
        cdf_stream_set(n, stream_base[n]);
        cdf_stream_step1(n);
    }
}

void display_finish(uint8_t colupf)
{
    finish(colupf, 0);
}

void display_finish_scene(void)
{
    finish(0, 1);
}

void display_pfcolor_line(unsigned line, uint8_t color)
{
    dd_put(DD_COLUPFL + line, color);
}
