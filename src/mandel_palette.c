#include "mandel_palette.h"

static uint8_t clamp_u32_to_u8(uint32_t value)
{
    return value > 255u ? 255u : (uint8_t)value;
}

void mandel_palette_rgb(uint32_t iter, uint32_t max_iter, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (r == 0 || g == 0 || b == 0) {
        return;
    }

    if (max_iter == 0 || iter >= max_iter) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    /*
     * A compact "fire and electric" gradient. The modulo bands keep common
     * Mandelbrot views from collapsing into one flat blue range.
     */
    const uint32_t band = iter % 64u;
    const uint32_t ramp = band * 4u;

    if (band < 16u) {
        *r = 0;
        *g = clamp_u32_to_u8(ramp * 2u);
        *b = clamp_u32_to_u8(80u + ramp * 2u);
    } else if (band < 32u) {
        *r = clamp_u32_to_u8((ramp - 64u) * 4u);
        *g = 255;
        *b = clamp_u32_to_u8(255u - (ramp - 64u) * 2u);
    } else if (band < 48u) {
        *r = 255;
        *g = clamp_u32_to_u8(255u - (ramp - 128u) * 3u);
        *b = 0;
    } else {
        *r = clamp_u32_to_u8(255u - (ramp - 192u) * 3u);
        *g = 0;
        *b = clamp_u32_to_u8((ramp - 192u) * 2u);
    }
}
