#include "mandel_render.h"

#include <assert.h>
#include <stdint.h>

static void test_rejects_invalid_inputs(void)
{
    uint32_t pixel = 42;
    const MandelView valid = {
        .width = 1,
        .height = 1,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 1.0,
        .max_iter = 10,
    };
    const MandelView invalid_width = {
        .width = 0,
        .height = 1,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 1.0,
        .max_iter = 10,
    };
    const MandelView invalid_height = {
        .width = 1,
        .height = 0,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 1.0,
        .max_iter = 10,
    };
    const MandelView invalid_scale = {
        .width = 1,
        .height = 1,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 0.0,
        .max_iter = 10,
    };
    const MandelView invalid_iter = {
        .width = 1,
        .height = 1,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 1.0,
        .max_iter = 0,
    };

    assert(mandel_render_f64(0, &pixel) == -1);
    assert(mandel_render_f64(&valid, 0) == -1);
    assert(mandel_render_f64(&invalid_width, &pixel) == -1);
    assert(mandel_render_f64(&invalid_height, &pixel) == -1);
    assert(mandel_render_f64(&invalid_scale, &pixel) == -1);
    assert(mandel_render_f64(&invalid_iter, &pixel) == -1);

    assert(mandel_render_region_f64(0, &pixel, 0, 1) == -1);
    assert(mandel_render_region_f64(&valid, 0, 0, 1) == -1);
    assert(mandel_render_region_f64(&valid, &pixel, -1, 1) == -1);
    assert(mandel_render_region_f64(&valid, &pixel, 1, 0) == -1);
    assert(mandel_render_region_f64(&valid, &pixel, 0, 2) == -1);
}

static void test_renders_single_center_pixel(void)
{
    uint32_t pixel = 0;
    const MandelView view = {
        .width = 1,
        .height = 1,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 4.0,
        .max_iter = 64,
    };

    assert(mandel_render_f64(&view, &pixel) == 0);
    assert(pixel == 64);
}

static void test_renders_full_buffer_deterministically(void)
{
    uint32_t first[12] = {0};
    uint32_t second[12] = {0};
    const MandelView view = {
        .width = 4,
        .height = 3,
        .center_re = -0.5,
        .center_im = 0.0,
        .scale = 3.0,
        .max_iter = 32,
    };

    assert(mandel_render_f64(&view, first) == 0);
    assert(mandel_render_f64(&view, second) == 0);

    for (int i = 0; i < 12; ++i) {
        assert(first[i] == second[i]);
        assert(first[i] <= 32);
    }
}

static void test_region_full_height_matches_full_render(void)
{
    uint32_t full[35] = {0};
    uint32_t region[35] = {0};
    const MandelView view = {
        .width = 7,
        .height = 5,
        .center_re = -0.75,
        .center_im = 0.1,
        .scale = 2.5,
        .max_iter = 64,
    };

    assert(mandel_render_f64(&view, full) == 0);
    assert(mandel_render_region_f64(&view, region, 0, view.height) == 0);

    for (int i = 0; i < 35; ++i) {
        assert(full[i] == region[i]);
    }
}

static void test_regions_can_reconstruct_full_render(void)
{
    uint32_t full[48] = {0};
    uint32_t chunked[48] = {0};
    const MandelView view = {
        .width = 8,
        .height = 6,
        .center_re = -0.5,
        .center_im = 0.0,
        .scale = 3.0,
        .max_iter = 80,
    };

    assert(mandel_render_f64(&view, full) == 0);
    assert(mandel_render_region_f64(&view, chunked, 0, 2) == 0);
    assert(mandel_render_region_f64(&view, chunked, 2, 5) == 0);
    assert(mandel_render_region_f64(&view, chunked, 5, 6) == 0);

    for (int i = 0; i < 48; ++i) {
        assert(full[i] == chunked[i]);
    }
}

static void test_empty_region_is_valid_and_does_not_write(void)
{
    uint32_t pixels[4] = {11, 22, 33, 44};
    const MandelView view = {
        .width = 2,
        .height = 2,
        .center_re = 0.0,
        .center_im = 0.0,
        .scale = 4.0,
        .max_iter = 16,
    };

    assert(mandel_render_region_f64(&view, pixels, 1, 1) == 0);
    assert(pixels[0] == 11);
    assert(pixels[1] == 22);
    assert(pixels[2] == 33);
    assert(pixels[3] == 44);
}

int main(void)
{
    test_rejects_invalid_inputs();
    test_renders_single_center_pixel();
    test_renders_full_buffer_deterministically();
    test_region_full_height_matches_full_render();
    test_regions_can_reconstruct_full_render();
    test_empty_region_is_valid_and_does_not_write();

    return 0;
}
