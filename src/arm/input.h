/*
 * Player 0 controls. The 6507 copies SWCHA and INPT4 into display data each
 * frame before CALLFN (see kernel.asm); this turns them into active-high bits.
 */
#ifndef INPUT_H
#define INPUT_H

#include "cdfj.h"

#define PAD_RIGHT   0x01u
#define PAD_LEFT    0x02u
#define PAD_DOWN    0x04u
#define PAD_UP      0x08u
#define PAD_FIRE    0x10u

/* VBLANK time left after the last frame's ARM work, in 64-cycle units; 0 if it ran out. */
static inline unsigned input_headroom(void)
{
    const unsigned v = CDF_DD[DD_IN_HEADROOM];

    return v >= 128u ? 0u : v;
}

static inline unsigned input_read(void)
{
    unsigned swcha = CDF_DD[DD_IN_SWCHA];
    unsigned pad = 0;

    if (!(swcha & 0x80u)) pad |= PAD_RIGHT;
    if (!(swcha & 0x40u)) pad |= PAD_LEFT;
    if (!(swcha & 0x20u)) pad |= PAD_DOWN;
    if (!(swcha & 0x10u)) pad |= PAD_UP;
    if (!(CDF_DD[DD_IN_INPT4] & 0x80u)) pad |= PAD_FIRE;
    return pad;
}

#endif /* INPUT_H */
