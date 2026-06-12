#include "mandel_render.h"

#include "mandel_core.h"

#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    const MandelView *view;
    uint32_t *iterations;
    int y_start;
    int y_end;
    int result;
} MandelThreadJob;

typedef struct {
    const MandelView *view;
    uint32_t *iterations;
    int tile_size;
    int tile_cols;
    int tile_count;
    int next_tile;
    pthread_mutex_t mutex;
} MandelTileQueue;

typedef struct {
    MandelTileQueue *queue;
    int result;
} MandelTileThreadJob;

static int mandel_view_is_valid(const MandelView *view)
{
    return view != 0 &&
        view->width > 0 &&
        view->height > 0 &&
        view->scale > 0.0 &&
        view->max_iter > 0;
}

static int mandel_tile_is_valid(const MandelView *view, const MandelTile *tile)
{
    return tile != 0 &&
        tile->x >= 0 &&
        tile->y >= 0 &&
        tile->width > 0 &&
        tile->height > 0 &&
        tile->width <= view->width - tile->x &&
        tile->height <= view->height - tile->y;
}

static void mandel_render_rect_f64(
    const MandelView *view,
    uint32_t *iterations,
    int x_start,
    int y_start,
    int x_end,
    int y_end)
{
    /*
     * scale is the horizontal span in the complex plane. The vertical span is
     * derived from the image aspect ratio so a square in math space does not
     * become stretched on rectangular images.
     */
    const double complex_width = view->scale;
    const double complex_height = view->scale * (double)view->height / (double)view->width;
    const double min_re = view->center_re - complex_width * 0.5;
    const double max_im = view->center_im + complex_height * 0.5;

    /*
     * Only the selected rectangle is written. This function is the shared core
     * for row regions and tiles, so both work units use exactly the same pixel
     * mapping and escape calculation.
     */
    for (int y = y_start; y < y_end; ++y) {
        /* Sample at pixel centers; y grows downward while imaginary values grow upward. */
        const double ci = max_im - ((double)y + 0.5) * complex_height / (double)view->height;

        for (int x = x_start; x < x_end; ++x) {
            /* Convert the pixel column into the real component of c. */
            const double cr = min_re + ((double)x + 0.5) * complex_width / (double)view->width;
            const int iter = mandel_escape_f64(cr, ci, view->max_iter);

            iterations[(size_t)y * (size_t)view->width + (size_t)x] = (uint32_t)iter;
        }
    }
}

static void *mandel_render_thread_main(void *arg)
{
    MandelThreadJob *job = arg;

    /*
     * The thread body is deliberately tiny: all pixel math stays in the region
     * renderer. The only thread-specific information is which rows this worker
     * owns.
     */
    job->result = mandel_render_region_f64(job->view, job->iterations, job->y_start, job->y_end);
    return 0;
}

static int mandel_tile_queue_take(MandelTileQueue *queue, MandelTile *tile)
{
    int tile_index = 0;

    /*
     * The queue's shared state is only next_tile. Keep the critical section as
     * small as possible: claim one tile index, then release the mutex before
     * doing any Mandelbrot work.
     */
    if (pthread_mutex_lock(&queue->mutex) != 0) {
        return -1;
    }

    if (queue->next_tile >= queue->tile_count) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    tile_index = queue->next_tile++;

    if (pthread_mutex_unlock(&queue->mutex) != 0) {
        return -1;
    }

    const int tile_row = tile_index / queue->tile_cols;
    const int tile_col = tile_index % queue->tile_cols;
    const int x = tile_col * queue->tile_size;
    const int y = tile_row * queue->tile_size;
    const int remaining_width = queue->view->width - x;
    const int remaining_height = queue->view->height - y;

    *tile = (MandelTile){
        .x = x,
        .y = y,
        .width = remaining_width < queue->tile_size ? remaining_width : queue->tile_size,
        .height = remaining_height < queue->tile_size ? remaining_height : queue->tile_size,
    };
    return 1;
}

static void *mandel_render_tile_thread_main(void *arg)
{
    MandelTileThreadJob *job = arg;
    MandelTile tile;

    /*
     * Dynamic scheduling loop: each worker keeps asking the shared queue for
     * work until no tiles remain. Slow tiles no longer pin one fixed thread to
     * one fixed image band; the next available worker can help with later work.
     */
    for (;;) {
        const int take_result = mandel_tile_queue_take(job->queue, &tile);

        if (take_result < 0) {
            job->result = -1;
            return 0;
        }

        if (take_result == 0) {
            job->result = 0;
            return 0;
        }

        if (mandel_render_tile_f64(job->queue->view, job->queue->iterations, &tile) != 0) {
            job->result = -1;
            return 0;
        }
    }
}

int mandel_render_f64(const MandelView *view, uint32_t *iterations)
{
    if (!mandel_view_is_valid(view)) {
        return -1;
    }

    return mandel_render_region_f64(view, iterations, 0, view->height);
}

int mandel_render_region_f64(const MandelView *view, uint32_t *iterations, int y_start, int y_end)
{
    if (!mandel_view_is_valid(view) || iterations == 0) {
        return -1;
    }

    if (y_start < 0 || y_end < y_start || y_end > view->height) {
        return -1;
    }

    mandel_render_rect_f64(view, iterations, 0, y_start, view->width, y_end);
    return 0;
}

int mandel_render_tile_f64(const MandelView *view, uint32_t *iterations, const MandelTile *tile)
{
    if (!mandel_view_is_valid(view) || iterations == 0 || !mandel_tile_is_valid(view, tile)) {
        return -1;
    }

    /*
     * A tile writes into its real image position, not into a compact temporary
     * buffer. That is what will let many tiles, computed in any order, assemble
     * the final image in the shared iterations array.
     */
    mandel_render_rect_f64(
        view,
        iterations,
        tile->x,
        tile->y,
        tile->x + tile->width,
        tile->y + tile->height);
    return 0;
}

int mandel_render_f64_threads(const MandelView *view, uint32_t *iterations, int thread_count)
{
    if (!mandel_view_is_valid(view) || iterations == 0 || thread_count <= 0) {
        return -1;
    }

    if (thread_count == 1) {
        return mandel_render_region_f64(view, iterations, 0, view->height);
    }

    /*
     * More threads than rows would create empty jobs. Clamp to height so every
     * created worker owns at least one row.
     */
    if (thread_count > view->height) {
        thread_count = view->height;
    }

    pthread_t *threads = malloc((size_t)thread_count * sizeof(*threads));
    MandelThreadJob *jobs = malloc((size_t)thread_count * sizeof(*jobs));

    if (threads == 0 || jobs == 0) {
        free(threads);
        free(jobs);
        return -1;
    }

    int created = 0;
    int result = 0;

    for (int i = 0; i < thread_count; ++i) {
        /*
         * Integer division creates non-overlapping half-open row ranges and
         * distributes remainder rows across the bands. Adjacent jobs meet at
         * the same boundary: one ends at y, the next starts at y.
         */
        const int y_start = (i * view->height) / thread_count;
        const int y_end = ((i + 1) * view->height) / thread_count;

        jobs[i] = (MandelThreadJob){
            .view = view,
            .iterations = iterations,
            .y_start = y_start,
            .y_end = y_end,
            .result = -1,
        };

        if (pthread_create(&threads[i], 0, mandel_render_thread_main, &jobs[i]) != 0) {
            result = -1;
            break;
        }

        ++created;
    }

    for (int i = 0; i < created; ++i) {
        if (pthread_join(threads[i], 0) != 0) {
            result = -1;
        }

        if (jobs[i].result != 0) {
            result = -1;
        }
    }

    free(threads);
    free(jobs);
    return result;
}

int mandel_render_f64_tile_threads(const MandelView *view, uint32_t *iterations, int thread_count, int tile_size)
{
    if (!mandel_view_is_valid(view) || iterations == 0 || thread_count <= 0 || tile_size <= 0) {
        return -1;
    }

    const int tile_cols = (view->width + tile_size - 1) / tile_size;
    const int tile_rows = (view->height + tile_size - 1) / tile_size;
    const int tile_count = tile_cols * tile_rows;

    if (thread_count > tile_count) {
        thread_count = tile_count;
    }

    pthread_t *threads = malloc((size_t)thread_count * sizeof(*threads));
    MandelTileThreadJob *jobs = malloc((size_t)thread_count * sizeof(*jobs));

    if (threads == 0 || jobs == 0) {
        free(threads);
        free(jobs);
        return -1;
    }

    MandelTileQueue queue = {
        .view = view,
        .iterations = iterations,
        .tile_size = tile_size,
        .tile_cols = tile_cols,
        .tile_count = tile_count,
        .next_tile = 0,
    };

    if (pthread_mutex_init(&queue.mutex, 0) != 0) {
        free(threads);
        free(jobs);
        return -1;
    }

    int created = 0;
    int result = 0;

    for (int i = 0; i < thread_count; ++i) {
        jobs[i] = (MandelTileThreadJob){
            .queue = &queue,
            .result = -1,
        };

        if (pthread_create(&threads[i], 0, mandel_render_tile_thread_main, &jobs[i]) != 0) {
            result = -1;
            break;
        }

        ++created;
    }

    for (int i = 0; i < created; ++i) {
        if (pthread_join(threads[i], 0) != 0) {
            result = -1;
        }

        if (jobs[i].result != 0) {
            result = -1;
        }
    }

    if (pthread_mutex_destroy(&queue.mutex) != 0) {
        result = -1;
    }

    free(threads);
    free(jobs);
    return result;
}
