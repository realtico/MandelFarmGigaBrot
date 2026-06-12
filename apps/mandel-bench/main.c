#include "mandel_render.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *name;
    MandelView view;
} BenchScene;

typedef struct {
    MandelView view;
    const char *scene_name;
    int json;
} BenchOptions;

static const BenchScene BENCH_SCENES[] = {
    {
        .name = "easy",
        .view = {
            .width = 1024,
            .height = 768,
            .center_re = 1.0,
            .center_im = 1.0,
            .scale = 3.0,
            .max_iter = 300,
        },
    },
    {
        .name = "medium",
        .view = {
            .width = 1024,
            .height = 768,
            .center_re = -0.5,
            .center_im = 0.0,
            .scale = 3.0,
            .max_iter = 1000,
        },
    },
    {
        .name = "hard",
        .view = {
            .width = 1024,
            .height = 768,
            .center_re = -0.743643887037151,
            .center_im = 0.13182590420533,
            .scale = 0.0025,
            .max_iter = 2000,
        },
    },
};

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
        "Usage: %s [--scene easy|medium|hard] [--width N] [--height N] [--max-iter N] [--json]\n"
        "          [--center-re X --center-im Y --scale X]\n",
        program);
}

static int parse_int_arg(const char *value, int *out)
{
    char *end = 0;
    long parsed = 0;

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

static const BenchScene *find_scene(const char *name)
{
    const size_t scene_count = sizeof(BENCH_SCENES) / sizeof(BENCH_SCENES[0]);

    for (size_t i = 0; i < scene_count; ++i) {
        if (strcmp(BENCH_SCENES[i].name, name) == 0) {
            return &BENCH_SCENES[i];
        }
    }

    return 0;
}

static int apply_scene(BenchOptions *options, const char *scene_name)
{
    const BenchScene *scene = find_scene(scene_name);

    if (scene == 0) {
        fprintf(stderr, "unknown scene: %s\n", scene_name);
        return -1;
    }

    options->scene_name = scene->name;
    options->view = scene->view;
    return 0;
}

static int parse_options(int argc, char **argv, BenchOptions *options)
{
    if (apply_scene(options, "medium") != 0) {
        return -1;
    }
    options->json = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scene") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || apply_scene(options, argv[++i]) != 0) {
                return -1;
            }
        }
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--json") == 0) {
            options->json = 1;
        } else if (strcmp(argv[i], "--scene") == 0) {
            if (require_value(i, argc, argv[i]) != 0) {
                return -1;
            }
            ++i;
        } else if (strcmp(argv[i], "--width") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.width) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--height") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.height) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--max-iter") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->view.max_iter) != 0) {
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
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    if (options->view.width <= 0 || options->view.height <= 0 || options->view.scale <= 0.0 || options->view.max_iter <= 0) {
        fprintf(stderr, "width, height, scale, and max-iter must be positive\n");
        return -1;
    }

    return 0;
}

static double monotonic_seconds(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
    struct timespec ts;

    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
        return 0.0;
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

static uint64_t sum_iterations(const uint32_t *iterations, size_t count)
{
    uint64_t total = 0;

    for (size_t i = 0; i < count; ++i) {
        total += iterations[i];
    }

    return total;
}

static void print_human_report(const BenchOptions *options, double duration_ms, double pixels_s, double iterations_s, uint64_t total_iterations)
{
    printf("MandelFarmGigaBrot benchmark\n");
    printf("  bench_version: mandelbench.0.1\n");
    printf("  backend: scalar_f64\n");
    printf("  scene: %s\n", options->scene_name);
    printf("  resolution: %dx%d\n", options->view.width, options->view.height);
    printf("  center: %.15g, %.15g\n", options->view.center_re, options->view.center_im);
    printf("  scale: %.15g\n", options->view.scale);
    printf("  max_iter: %d\n", options->view.max_iter);
    printf("  duration_ms: %.3f\n", duration_ms);
    printf("  pixels_s: %.3f\n", pixels_s);
    printf("  total_iterations: %llu\n", (unsigned long long)total_iterations);
    printf("  iterations_s: %.3f\n", iterations_s);
}

static void print_json_report(const BenchOptions *options, double duration_ms, double pixels_s, double iterations_s, uint64_t total_iterations)
{
    printf("{\n");
    printf("  \"project\": \"MandelFarmGigaBrot\",\n");
    printf("  \"bench_version\": \"mandelbench.0.1\",\n");
    printf("  \"backend\": \"scalar_f64\",\n");
    printf("  \"scene\": \"%s\",\n", options->scene_name);
    printf("  \"width\": %d,\n", options->view.width);
    printf("  \"height\": %d,\n", options->view.height);
    printf("  \"center_re\": %.17g,\n", options->view.center_re);
    printf("  \"center_im\": %.17g,\n", options->view.center_im);
    printf("  \"scale\": %.17g,\n", options->view.scale);
    printf("  \"max_iter\": %d,\n", options->view.max_iter);
    printf("  \"duration_ms\": %.6f,\n", duration_ms);
    printf("  \"pixels_s\": %.6f,\n", pixels_s);
    printf("  \"total_iterations\": %llu,\n", (unsigned long long)total_iterations);
    printf("  \"iterations_s\": %.6f\n", iterations_s);
    printf("}\n");
}

int main(int argc, char **argv)
{
    BenchOptions options;

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

    const double start_s = monotonic_seconds();
    const int render_result = mandel_render_f64(&options.view, iterations);
    const double end_s = monotonic_seconds();

    if (render_result != 0 || end_s < start_s) {
        fprintf(stderr, "failed to benchmark Mandelbrot render\n");
        free(iterations);
        return 1;
    }

    const double duration_s = end_s - start_s;
    const double duration_ms = duration_s * 1000.0;
    const uint64_t total_iterations = sum_iterations(iterations, pixel_count);
    const double pixels_s = duration_s > 0.0 ? (double)pixel_count / duration_s : 0.0;
    const double iterations_s = duration_s > 0.0 ? (double)total_iterations / duration_s : 0.0;

    if (options.json) {
        print_json_report(&options, duration_ms, pixels_s, iterations_s, total_iterations);
    } else {
        print_human_report(&options, duration_ms, pixels_s, iterations_s, total_iterations);
    }

    free(iterations);
    return 0;
}
