#include "mandel_types.h"

#include <assert.h>

static void test_mandel_view_fields_are_assignable(void)
{
    const MandelView view = {
        .width = 1024,
        .height = 768,
        .center_re = -0.5,
        .center_im = 0.0,
        .scale = 3.0,
        .max_iter = 1000,
    };

    assert(view.width == 1024);
    assert(view.height == 768);
    assert(view.center_re == -0.5);
    assert(view.center_im == 0.0);
    assert(view.scale == 3.0);
    assert(view.max_iter == 1000);
}

int main(void)
{
    test_mandel_view_fields_are_assignable();

    return 0;
}
