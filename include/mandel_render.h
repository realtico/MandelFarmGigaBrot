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

/*
 * Renders one rectangular tile of the requested view.
 *
 * The tile must fit entirely inside the image bounds described by view. Pixels
 * are still written into the full image buffer, not into a compact tile-local
 * buffer. This keeps tile rendering composable: rendering all tiles into the
 * same output buffer should reconstruct the same image as mandel_render_f64().
 *
 * Returns 0 on success and -1 when the view, output buffer, or tile rectangle
 * is invalid.
 */
int mandel_render_tile_f64(const MandelView *view, uint32_t *iterations, const MandelTile *tile);

/*
 * Renders the full view using horizontal row bands distributed across threads.
 *
 * Each worker receives a non-overlapping [y_start, y_end) interval and writes
 * only that interval into the shared output buffer. The shared MandelView is
 * read-only. This keeps the first threaded renderer free of data races without
 * needing locks around pixel writes.
 *
 * thread_count must be positive. Passing 1 is valid and should produce exactly
 * the same result as mandel_render_f64().
 */
int mandel_render_f64_threads(const MandelView *view, uint32_t *iterations, int thread_count);

/*
 * Renders the full view using a shared queue of rectangular tiles.
 *
 * Unlike mandel_render_f64_threads(), work is not assigned as one fixed row
 * band per thread. Instead, many tiles are generated from tile_size, and each
 * worker repeatedly takes the next tile from a mutex-protected queue. This is
 * the first dynamic scheduler in the project and prepares the mental model for
 * future local and remote workers.
 *
 * thread_count and tile_size must be positive. Passing 1 thread is valid and
 * still uses the same tile path, just without parallel execution.
 */
int mandel_render_f64_tile_threads(const MandelView *view, uint32_t *iterations, int thread_count, int tile_size);

#endif
