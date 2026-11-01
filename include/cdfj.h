/*
 * CDFJ+ driver interface, ARM side.
 *
 * The 6507 reads data through datastreams ("fast fetch"). Each stream has a
 * 32-bit pointer in driver RAM: idx = ptr >> 16 is the display-data offset of
 * the next byte. Each stream also has an increment word; its low 16 bits are
 * 8.8 fixed point, so 0x0100 advances one byte per fetch.
 *
 * ARM sets the pointers in VBLANK, once per frame, before the kernel runs.
 */
#ifndef CDFJ_H
#define CDFJ_H

#include <stdint.h>
#include "layout.h"

#define CDF_DS_PTR(n) (*(volatile uint32_t *)(DRV_PTR_ARM + 4u * (uint32_t)(n)))
#define CDF_DS_INC(n) (*(volatile uint32_t *)(DRV_INC_ARM + 4u * (uint32_t)(n)))

/* Display data as bytes. Index with a DD_* offset from layout.h. */
#define CDF_DD ((volatile uint8_t *)DD_BASE_ARM)

/* Point stream n at display-data offset dd_off. */
static inline void cdf_stream_set(unsigned n, unsigned dd_off)
{
    CDF_DS_PTR(n) = (uint32_t)dd_off << 16;
}

/* Advance stream n by one byte per fetch. */
static inline void cdf_stream_step1(unsigned n)
{
    CDF_DS_INC(n) = 0x0100u;
}

#endif /* CDFJ_H */
