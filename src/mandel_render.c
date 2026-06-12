#include "mandel_render.h"

#include "mandel_core.h"

#include <stddef.h>

static int mandel_view_is_valid(const MandelView *view)
{
    return view != 0 &&
        view->width > 0 &&
        view->height > 0 &&
        view->scale > 0.0 &&
        view->max_iter > 0;
}

int mandel_render_f64(const MandelView *view, uint32_t *iterations)
{
    if (!mandel_view_is_valid(view) || iterations == 0) {
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

    for (int y = 0; y < view->height; ++y) {
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
