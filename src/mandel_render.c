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

static int mandel_view_is_valid(const MandelView *view)
{
    return view != 0 &&
        view->width > 0 &&
        view->height > 0 &&
        view->scale > 0.0 &&
        view->max_iter > 0;
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
     * Only the selected rows are written. This is what makes region rendering a
     * safe building block for future threads: if two regions do not overlap,
     * they do not write to the same buffer positions.
     */
    for (int y = y_start; y < y_end; ++y) {
        /* Sample at pixel centers; y grows downward while imaginary values grow upward. */
        const double ci = max_im - ((double)y + 0.5) * complex_height / (double)view->height;

        for (int x = 0; x < view->width; ++x) {
            /* Convert the pixel column into the real component of c. */
            const double cr = min_re + ((double)x + 0.5) * complex_width / (double)view->width;
            const int iter = mandel_escape_f64(cr, ci, view->max_iter);

            iterations[(size_t)y * (size_t)view->width + (size_t)x] = (uint32_t)iter;
        }
    }

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
