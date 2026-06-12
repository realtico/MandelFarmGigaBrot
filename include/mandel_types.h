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

/*
 * Describes a rectangular pixel region inside a MandelView.
 *
 * x and y are measured from the top-left corner of the output image. width and
 * height are measured in pixels. Tiles are the next step after horizontal row
 * regions: they are small work units that can later be placed in a queue and
 * consumed by local or remote workers.
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} MandelTile;

#endif
