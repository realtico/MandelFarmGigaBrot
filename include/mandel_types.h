#ifndef MANDEL_TYPES_H
#define MANDEL_TYPES_H

/*
 * Describes which rectangle of the complex plane should be rendered.
 *
 * scale is the complex-plane width. The complex-plane height is derived from
 * width/height so pixels keep the same aspect ratio as the requested image.
 */
typedef struct {
    int width;
    int height;
    double center_re;
    double center_im;
    double scale;
    int max_iter;
} MandelView;

#endif
