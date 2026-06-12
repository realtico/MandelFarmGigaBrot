#include "mandel_core.h"

int mandel_escape_f64(double cr, double ci, int max_iter)
{
    double zr = 0.0;
    double zi = 0.0;

    if (max_iter <= 0) {
        return 0;
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        const double zr2 = zr * zr;
        const double zi2 = zi * zi;

        if (zr2 + zi2 > 4.0) {
            return iter;
        }

        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }

    return max_iter;
}
