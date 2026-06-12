#ifndef MANDEL_TYPES_H
#define MANDEL_TYPES_H

typedef struct {
    int width;
    int height;
    double center_re;
    double center_im;
    double scale;
    int max_iter;
} MandelView;

#endif
