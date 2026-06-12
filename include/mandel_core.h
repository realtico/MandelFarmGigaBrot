#ifndef MANDEL_CORE_H
#define MANDEL_CORE_H

/*
 * Returns how many iterations the complex point c = cr + ci*i takes to escape
 * the Mandelbrot recurrence z(n+1) = z(n)^2 + c.
 *
 * If the point does not escape before max_iter, the function returns max_iter.
 * A returned value smaller than max_iter means the point is outside the set.
 */
int mandel_escape_f64(double cr, double ci, int max_iter);

#endif
