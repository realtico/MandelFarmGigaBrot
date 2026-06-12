#ifndef MANDEL_PALETTE_H
#define MANDEL_PALETTE_H

#include <stdint.h>

/*
 * Converts an escape iteration count to RGB.
 *
 * Points that reach max_iter are treated as inside the set and become black.
 * Escaped points receive a visible cyclic gradient suitable for PPM output and
 * the Nanquim demo.
 */
void mandel_palette_rgb(uint32_t iter, uint32_t max_iter, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
