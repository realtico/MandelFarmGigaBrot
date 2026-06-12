#include "mandel_palette.h"
#include "mandel_render.h"

#include "nanquim.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    /* Keep these constants near the top so the demo resolution is easy to tweak. */
    DEMO_WIDTH = 1024,
    DEMO_HEIGHT = 768,
    DEMO_MAX_ITER = 256,
};

static void draw_iterations(const uint32_t *iterations, int width, int height, uint32_t max_iter)
{
    /*
     * The renderer produces one escape-count value per pixel. Nanquim draws
     * colored pixels, so this pass converts each iteration count to RGB and
     * plots it at the matching screen coordinate.
     */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            const uint32_t iter = iterations[(size_t)y * (size_t)width + (size_t)x];

            mandel_palette_rgb(iter, max_iter, &r, &g, &b);
            nq_color(r, g, b);
            nq_pset((float)x, (float)y);
        }
    }
}

int main(void)
{
    /*
     * A classic Mandelbrot viewport: centered near the full set, with scale as
     * the complex-plane width. Height is derived by mandel_render_f64 from the
     * image aspect ratio.
     */
    const MandelView view = {
        .width = DEMO_WIDTH,
        .height = DEMO_HEIGHT,
        .center_re = -0.5,
        .center_im = 0.0,
        .scale = 3.0,
        .max_iter = DEMO_MAX_ITER,
    };
    const size_t pixel_count = (size_t)view.width * (size_t)view.height;

    /*
     * The render layer does not allocate internally. The caller owns the
     * iteration buffer so future apps can reuse memory, tile it, or hand it to
     * another system without hidden allocation policy.
     */
    uint32_t *iterations = malloc(pixel_count * sizeof(*iterations));

    if (iterations == 0) {
        fprintf(stderr, "failed to allocate Mandelbrot iteration buffer\n");
        return 1;
    }

    if (mandel_render_f64(&view, iterations) != 0) {
        fprintf(stderr, "failed to render Mandelbrot view\n");
        free(iterations);
        return 1;
    }

    /*
     * Nanquim is only the visualization layer here. The Mandelbrot math and
     * buffer renderer are already done before the window opens.
     */
    if (nq_screen(view.width, view.height, "MandelFarmGigaBrot - Mandelbrot Demo", NQ_SCALE_FIXED) < 0) {
        fprintf(stderr, "failed to open Nanquim window\n");
        free(iterations);
        return 1;
    }

    nq_setup_coords(0.0f, (float)view.width, 0.0f, (float)view.height);
    nq_background(0, 0, 0);
    draw_iterations(iterations, view.width, view.height, (uint32_t)view.max_iter);
    nq_sync_all();

    /* Static image: just keep processing window events until the user closes it. */
    while (nq_running()) {
        nq_poll_events();
        SDL_Delay(16);
    }

    nq_close();
    free(iterations);

    return 0;
}
