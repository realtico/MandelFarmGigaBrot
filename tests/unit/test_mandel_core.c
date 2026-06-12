#include "mandel_core.h"

#include <assert.h>

static void test_points_inside_or_stable_until_limit(void)
{
    assert(mandel_escape_f64(0.0, 0.0, 1000) == 1000);
    assert(mandel_escape_f64(-1.0, 0.0, 1000) == 1000);
}

static void test_point_outside_escapes_quickly(void)
{
    const int iter = mandel_escape_f64(2.0, 2.0, 1000);

    assert(iter > 0);
    assert(iter < 4);
}

static void test_invalid_iteration_limit(void)
{
    assert(mandel_escape_f64(0.0, 0.0, 0) == 0);
    assert(mandel_escape_f64(0.0, 0.0, -1) == 0);
}

int main(void)
{
    test_points_inside_or_stable_until_limit();
    test_point_outside_escapes_quickly();
    test_invalid_iteration_limit();

    return 0;
}
