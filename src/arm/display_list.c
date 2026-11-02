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
 * Sprites: every object (the hero and each enemy) is 16x16, drawn with the
 * P0 + P1 pair. The kernel does HMOVE on every line, so the pair drifts from one
 * object's X to the next one's during the blank lines between them (up to 8 px
 * right or 7 px left per line). Objects too close vertically for that drift
 * take turns: the one skipped longest is drawn, so they flicker instead of
 * vanishing.
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
#include "level.h"
#include "physics.h"
#include "shots.h"

/* Stream n -> display-data table. Order matches STREAM_* in layout.inc. */
static const uint16_t stream_base[NUM_STREAMS] = {
    DD_PF0L, DD_PF1L, DD_PF2L, DD_PF0R, DD_PF1R, DD_PF2R,
    DD_GRP0, DD_GRP1, DD_HM, DD_COLUP, DD_BK, DD_COLUPF, DD_TOPX,
    DD_ENAM0, DD_M0X, DD_COLUPFL, DD_MODE
};

#define SCREEN_COLS   40
#define SCREEN_W      (SCREEN_COLS * TILE_W)
#define CAMERA_MAX    (LEVEL_W_PX - SCREEN_W)
#define HERO_SCREEN_X 72            /* where the camera keeps the hero, mid-level */
#define OBJ_W         16
#define OBJ_H         16
#define OBJ_X_MAX     (SCREEN_W - OBJ_W)
#define MAX_OBJS      (1 + 16)      /* the hero, then up to 16 enemies */

/* PosObject in kernel.asm places a sprite this many pixels left of the value
 * it is given; measured with tools/snap.py. */
#define HERO_X_TRIM 3
#define SHOT_X_TRIM 4               /* the same for missile 0 */

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

/* Can `next` be drawn after `prev` with the same sprite pair? */
static int fits_after(const obj_t *prev, const obj_t *next)
{
    const int first_free = prev->top + OBJ_H;          /* first line after prev */
    const int gap = next->top - first_free;            /* blank lines between */
    const int start = first_free > 0 ? first_free : 0;

    /* One blank line to switch colors, and enough HMOVE lines (the gap plus
     * next's first line, whose HMOVE runs before it is drawn) to drift over. */
    return gap >= 1 && next->top - start + 1 >= drift_lines(next->x - prev->x);
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

/*
 * Choose which objects to draw this frame, in top-to-bottom order. When one
 * doesn't fit after the last chosen object, the one that has waited longer
 * wins. Returns the number chosen; updates starve[].
 */
static int schedule(const obj_t *objs, int n, obj_t *out)
{
    int m = 0, i;

    for (i = 0; i < n; i++)
        if (starve[objs[i].id] < 255)
            starve[objs[i].id]++;

    for (i = 0; i < n; i++) {
        const obj_t *o = &objs[i];

        while (m > 0 && !fits_after(&out[m - 1], o)) {
            if (starve[o->id] > starve[out[m - 1].id])
                m--;                /* drop the previous one, try again */
            else
                break;
        }
        if (m == 0 || fits_after(&out[m - 1], o))
            out[m++] = *o;
    }

    for (i = 0; i < m; i++)
        starve[out[i].id] = 0;
    return m;
}

static uint8_t hm_byte(int move)    /* move > 0: right, < 0: left */
{
    if (move > 0)
        return (uint8_t)(((16 - move) & 15) << 4);
    return (uint8_t)((-move) << 4);
}

/* Mirror a 16-pixel row: the left byte becomes the reversed right byte and back. */
static uint16_t mirror16(uint16_t v)
{
    return (uint16_t)((fb_rev8[v & 0xFFu] << 8) | fb_rev8[v >> 8]);
}

static void build_sprites(const obj_t *s, int m)
{
    int i, k;
    unsigned line;

    for (line = 0; line < LINES; line++) {
        dd_put(DD_GRP0 + line, 0);
        dd_put(DD_GRP1 + line, 0);
    }
    for (line = 0; line <= LINES; line++) {
        dd_put(DD_HM + line, 0);
        dd_put(DD_COLUP + line, 0);
    }

    for (i = 0; i < m; i++) {
        const art_frame_t *f = &art_frames[s[i].frame];

        /* Graphics and color for each row that is on screen. The kernel reads
         * COLUP[y] at the end of line y-1, so every row has its own color. */
        for (k = 0; k < OBJ_H; k++) {
            const int y = s[i].top + k;

            if (y >= 0 && y < (int)LINES) {
                const uint16_t bits = s[i].flip ? mirror16(f->rows[k]) : f->rows[k];

                dd_put(DD_GRP0 + y, (uint8_t)(bits >> 8));
                dd_put(DD_GRP1 + y, (uint8_t)(bits & 0xFFu));
                dd_put(DD_COLUP + y, f->colors[k]);
            }
        }

        /* Drift to the next object over the blank lines after this one. */
        if (i + 1 < m) {
            int dx = s[i + 1].x - s[i].x;

            k = s[i].top + OBJ_H;
            if (k < 0)
                k = 0;
            while (dx != 0 && k <= (int)LINES) {
                int move = dx > 8 ? 8 : (dx < -7 ? -7 : dx);

                dd_put(DD_HM + k, hm_byte(move));
                dx -= move;
                k++;
            }
        }
    }

    dd_put(DD_TOPX, (uint8_t)((m > 0 ? s[0].x : 0) + HERO_X_TRIM));
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

static void clear_shot(void)
{
    unsigned line;

    for (line = 0; line <= LINES; line++)
        dd_put(DD_ENAM0 + line, 0);
    dd_put(DD_M0X, 0);
}

/* Missile 0 shows one bullet: enable it on its lines, and send its X. */
static void build_shot(int cam)
{
    int sx, sy, k;
    unsigned line;

    for (line = 0; line <= LINES; line++)
        dd_put(DD_ENAM0 + line, 0);

    if (!shots_pick(&sx, &sy)) {
        dd_put(DD_M0X, 0);
        return;
    }
    sx -= cam;
    if (sx < 0 || sx > SCREEN_W - SHOT_W) {
        dd_put(DD_M0X, 0);
        return;
    }
    for (k = 0; k < SHOT_H; k++) {
        const int y = sy + k;

        if (y >= 0 && y < (int)LINES)
            dd_put(DD_ENAM0 + y, 2);    /* ENAM0 D1 = on */
    }
    dd_put(DD_M0X, (uint8_t)(sx + SHOT_X_TRIM));
}

void display_build(void)
{
    const int cam = camera_x(physics_hero_x());
    const unsigned col0 = (unsigned)cam / TILE_W;
    obj_t objs[MAX_OBJS], shown[MAX_OBJS];
    unsigned row, line = 0;
    int m;

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

    m = schedule(objs, gather(objs, cam), shown);
    build_sprites(shown, m);
    build_shot(cam);

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
    obj_t objs[MAX_OBJS], shown[MAX_OBJS];
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
    build_sprites(shown, schedule(objs, n, shown));
    clear_shot();
}

void display_no_sprites(void)
{
    build_sprites(0, 0);
    clear_shot();
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
