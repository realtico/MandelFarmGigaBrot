#include "mandel_core.h"

int mandel_escape_f64(double cr, double ci, int max_iter)
{
    /* z starts at 0 + 0i for every Mandelbrot point. */
    double zr = 0.0;
    double zi = 0.0;

    if (max_iter <= 0) {
        return 0;
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        const double zr2 = zr * zr;
        const double zi2 = zi * zi;

        /*
         * A point has escaped once |z| > 2. We compare squared values here:
         * |z|^2 = zr^2 + zi^2, so the threshold is 2^2 = 4.
         */
        if (zr2 + zi2 > 4.0) {
            return iter;
        }

        /* Expand (zr + zi*i)^2 + (cr + ci*i) into real and imaginary parts. */
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }

    /* Reaching the limit means "treat as inside" at this precision/depth. */
    return max_iter;
}
