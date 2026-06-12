#ifndef MANDEL_RENDER_H
#define MANDEL_RENDER_H

#include "mandel_types.h"

#include <stdint.h>

int mandel_render_f64(const MandelView *view, uint32_t *iterations);

#endif
