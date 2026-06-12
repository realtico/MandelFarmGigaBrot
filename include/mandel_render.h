#ifndef MANDEL_RENDER_H
#define MANDEL_RENDER_H

#include "mandel_types.h"

#include <stdint.h>

/*
 * Renders the requested Mandelbrot view into a caller-owned buffer.
 *
 * The iterations buffer must have width * height entries. Each entry receives
 * the escape iteration count for one pixel, in row-major order.
 *
 * Returns 0 on success and -1 when view, dimensions, scale, max_iter, or the
 * output buffer are invalid.
 */
int mandel_render_f64(const MandelView *view, uint32_t *iterations);

#endif
