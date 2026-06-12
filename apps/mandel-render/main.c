#include "mandel_palette.h"
#include "mandel_render.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MandelView view;
    const char *output_path;
} RenderOptions;

/*
 * The render CLI is intentionally small: it converts command-line parameters
 * into a MandelView, asks the core renderer for raw iteration counts, then maps
 * those counts to RGB when writing the final image.
 */
static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
        "Usage: %s --width N --height N --center-re X --center-im Y --scale X --max-iter N --output FILE\n"
        "\n"
        "Defaults:\n"
        "  --width 1024 --height 768 --center-re -0.5 --center-im 0.0 --scale 3.0 --max-iter 1000\n",
        program);
}

static int parse_int_arg(const char *value, int *out)
{
    char *end = 0;
    long parsed = 0;

    /*
     * strtol lets us reject partial values such as "123abc". That keeps the CLI
     * predictable and avoids silently accepting typos in render dimensions.
     */
    errno = 0;
    parsed = strtol(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0' || parsed < -2147483647L || parsed > 2147483647L) {
        return -1;
    }

    *out = (int)parsed;
    return 0;
}

static int parse_double_arg(const char *value, double *out)
{
    char *end = 0;
    double parsed = 0.0;

    /*
     * View coordinates and scale are doubles because Mandelbrot navigation
     * quickly needs fractional positions in the complex plane.
     */
    errno = 0;
    parsed = strtod(value, &end);

    if (errno != 0 || end == value || *end != '\0') {
        return -1;
    }

    *out = parsed;
    return 0;
}

static int require_value(int index, int argc, const char *option)
{
    if (index + 1 >= argc) {
        fprintf(stderr, "%s requires a value\n", option);
        return -1;
    }

    return 0;
}

static int parse_options(int argc, char **argv, RenderOptions *options)
{
    /*
     * Defaults render the classic whole-set view. Every flag below overrides
     * just one field, which makes small experiments cheap from the terminal.
     */
    options->view = (MandelView){
        .width = 1024,
        .height = 768,
        .center_re = -0.5,
        .center_im = 0.0,
        .scale = 3.0,
        .max_iter = 1000,
    };
    options->output_path = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--width") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.width) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--height") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.height) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--center-re") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_double_arg(argv[++i], &options->view.center_re) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--center-im") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_double_arg(argv[++i], &options->view.center_im) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--scale") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_double_arg(argv[++i], &options->view.scale) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--max-iter") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.max_iter) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--output") == 0) {
            if (require_value(i, argc, argv[i]) != 0) {
                return -1;
            }
            options->output_path = argv[++i];
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    if (options->view.width <= 0 || options->view.height <= 0 || options->view.scale <= 0.0 || options->view.max_iter <= 0) {
        fprintf(stderr, "width, height, scale, and max-iter must be positive\n");
        return -1;
    }

    if (options->output_path == 0) {
        fprintf(stderr, "--output is required\n");
        return -1;
    }

    return 0;
}

static int write_ppm_p6(const char *path, const MandelView *view, const uint32_t *iterations)
{
    FILE *file = fopen(path, "wb");

    if (file == 0) {
        fprintf(stderr, "failed to open output file: %s\n", path);
        return -1;
    }

    if (fprintf(file, "P6\n%d %d\n255\n", view->width, view->height) < 0) {
        fclose(file);
        return -1;
    }

    /*
     * PPM P6 is deliberately simple: a short ASCII header followed by raw RGB
     * bytes. The renderer stores iteration counts, not colors, so this is the
     * final presentation step where the palette is applied.
     */
    for (int y = 0; y < view->height; ++y) {
        for (int x = 0; x < view->width; ++x) {
            uint8_t rgb[3] = {0, 0, 0};
            const uint32_t iter = iterations[(size_t)y * (size_t)view->width + (size_t)x];

            mandel_palette_rgb(iter, (uint32_t)view->max_iter, &rgb[0], &rgb[1], &rgb[2]);

            if (fwrite(rgb, sizeof(rgb), 1, file) != 1) {
                fclose(file);
                return -1;
            }
        }
    }

    if (fclose(file) != 0) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    RenderOptions options;

    if (parse_options(argc, argv, &options) != 0) {
        print_usage(stderr, argv[0]);
        return 2;
    }

    const size_t pixel_count = (size_t)options.view.width * (size_t)options.view.height;
    uint32_t *iterations = malloc(pixel_count * sizeof(*iterations));

    if (iterations == 0) {
        fprintf(stderr, "failed to allocate iteration buffer\n");
        return 1;
    }

    /*
     * Keep rendering and image encoding separate. Later backends can fill the
     * same iterations buffer without changing the PPM writer.
     */
    if (mandel_render_f64(&options.view, iterations) != 0) {
        fprintf(stderr, "failed to render Mandelbrot view\n");
        free(iterations);
        return 1;
    }

    if (write_ppm_p6(options.output_path, &options.view, iterations) != 0) {
        fprintf(stderr, "failed to write PPM output\n");
        free(iterations);
        return 1;
    }

    free(iterations);
    return 0;
}
