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

/*
 * Renders only a horizontal region of the requested view.
 *
 * The interval is half-open: y_start is included, y_end is excluded. For
 * example, y_start=10 and y_end=20 renders lines 10 through 19.
 *
 * This is the basic work unit for future multicore rendering. Multiple workers
 * can safely render different non-overlapping line ranges into the same buffer
 * because each range writes to different rows.
 *
 * Returns 0 on success and -1 when the view, output buffer, or row interval is
 * invalid.
 */
int mandel_render_region_f64(const MandelView *view, uint32_t *iterations, int y_start, int y_end);

#endif
