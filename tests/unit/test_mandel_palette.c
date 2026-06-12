#include "mandel_palette.h"

#include <assert.h>
#include <stdint.h>

static void test_inside_points_are_black(void)
{
    uint8_t r = 1;
    uint8_t g = 1;
    uint8_t b = 1;

    mandel_palette_rgb(100, 100, &r, &g, &b);

    assert(r == 0);
    assert(g == 0);
    assert(b == 0);
}

static void test_escaped_points_are_colored(void)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    mandel_palette_rgb(20, 100, &r, &g, &b);

    assert(r != 0 || g != 0 || b != 0);
}

static void test_null_outputs_are_ignored(void)
{
    uint8_t g = 123;
    uint8_t b = 45;

    mandel_palette_rgb(20, 100, 0, &g, &b);

    assert(g == 123);
    assert(b == 45);
}

int main(void)
{
    test_inside_points_are_black();
    test_escaped_points_are_colored();
    test_null_outputs_are_ignored();

    return 0;
}
