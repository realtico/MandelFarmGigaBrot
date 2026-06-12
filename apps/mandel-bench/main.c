#include "mandel_render.h"

#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const char *name;
    MandelView view;
} BenchScene;

typedef struct {
    MandelView view;
    const char *scene_name;
    int json;
    int node_report;
    const char *node_name;
    const char *output_path;
} BenchOptions;

typedef struct {
    char node_id[96];
    char node_name[96];
    char os[64];
    char arch[64];
    char hostname[96];
    int logical_cores;
} NodeInfo;

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
        "          [--center-re X --center-im Y --scale X]\n"
        "          [--node-report] [--node-name NAME] [--output FILE]\n",
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
    options->node_report = 0;
    options->node_name = 0;
    options->output_path = 0;

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
        } else if (strcmp(argv[i], "--node-report") == 0) {
            options->node_report = 1;
        } else if (strcmp(argv[i], "--scene") == 0) {
            if (require_value(i, argc, argv[i]) != 0) {
                return -1;
            }
            ++i;
        } else if (strcmp(argv[i], "--node-name") == 0) {
            if (require_value(i, argc, argv[i]) != 0) {
                return -1;
            }
            options->node_name = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0) {
            if (require_value(i, argc, argv[i]) != 0) {
                return -1;
            }
            options->output_path = argv[++i];
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

    if (options->node_report && !options->json) {
        fprintf(stderr, "--node-report currently requires --json\n");
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

static uint32_t fnv1a_update(uint32_t hash, const char *text)
{
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= 16777619u;
        ++text;
    }

    return hash;
}

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }

    snprintf(dst, dst_size, "%s", src != 0 && src[0] != '\0' ? src : "unknown");
}

static void make_node_id(char *dst, size_t dst_size, const char *node_name, uint32_t hash)
{
    size_t used = 0;

    if (dst_size == 0) {
        return;
    }

    for (const char *p = node_name; *p != '\0' && used + 1 < dst_size; ++p) {
        const unsigned char ch = (unsigned char)*p;

        if (isalnum(ch) || ch == '-' || ch == '_') {
            dst[used++] = (char)ch;
        } else if (used == 0 || dst[used - 1] != '-') {
            dst[used++] = '-';
        }
    }

    if (used == 0) {
        copy_string(dst, dst_size, "node");
        used = strlen(dst);
    } else {
        dst[used] = '\0';
    }

    snprintf(dst + used, dst_size - used, "-%04x", hash & 0xffffu);
}

static void print_json_string(FILE *stream, const char *text)
{
    fputc('"', stream);

    for (const char *p = text; p != 0 && *p != '\0'; ++p) {
        const unsigned char ch = (unsigned char)*p;

        if (ch == '"' || ch == '\\') {
            fputc('\\', stream);
            fputc(ch, stream);
        } else if (ch == '\n') {
            fputs("\\n", stream);
        } else if (ch == '\r') {
            fputs("\\r", stream);
        } else if (ch == '\t') {
            fputs("\\t", stream);
        } else if (ch < 0x20u) {
            fprintf(stream, "\\u%04x", ch);
        } else {
            fputc(ch, stream);
        }
    }

    fputc('"', stream);
}

static void collect_node_info(NodeInfo *node, const char *configured_name)
{
    struct utsname uts;
    char hostname[sizeof(node->hostname)];
    long cores = -1;

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        copy_string(hostname, sizeof(hostname), "unknown-host");
    }
    hostname[sizeof(hostname) - 1] = '\0';

    if (uname(&uts) == 0) {
        copy_string(node->os, sizeof(node->os), uts.sysname);
        copy_string(node->arch, sizeof(node->arch), uts.machine);
    } else {
        copy_string(node->os, sizeof(node->os), "unknown-os");
        copy_string(node->arch, sizeof(node->arch), "unknown-arch");
    }

    copy_string(node->hostname, sizeof(node->hostname), hostname);
    copy_string(node->node_name, sizeof(node->node_name), configured_name != 0 ? configured_name : hostname);

#ifdef _SC_NPROCESSORS_ONLN
    cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    node->logical_cores = cores > 0 ? (int)cores : 1;

    const uint32_t hash = fnv1a_update(fnv1a_update(2166136261u, node->hostname), node->arch) ^
        (uint32_t)getpid() ^
        (uint32_t)time(0);

    make_node_id(node->node_id, sizeof(node->node_id), node->node_name, hash);
}

static FILE *open_report_stream(const BenchOptions *options)
{
    if (options->output_path == 0) {
        return stdout;
    }

    FILE *file = fopen(options->output_path, "w");

    if (file == 0) {
        fprintf(stderr, "failed to open output file: %s\n", options->output_path);
    }

    return file;
}

static int close_report_stream(const BenchOptions *options, FILE *stream)
{
    if (stream == 0 || stream == stdout || options->output_path == 0) {
        return 0;
    }

    return fclose(stream) == 0 ? 0 : -1;
}

static void print_human_report(FILE *stream, const BenchOptions *options, double duration_ms, double pixels_s, double iterations_s, uint64_t total_iterations)
{
    fprintf(stream, "MandelFarmGigaBrot benchmark\n");
    fprintf(stream, "  bench_version: mandelbench.0.1\n");
    fprintf(stream, "  backend: scalar_f64\n");
    fprintf(stream, "  scene: %s\n", options->scene_name);
    fprintf(stream, "  resolution: %dx%d\n", options->view.width, options->view.height);
    fprintf(stream, "  center: %.15g, %.15g\n", options->view.center_re, options->view.center_im);
    fprintf(stream, "  scale: %.15g\n", options->view.scale);
    fprintf(stream, "  max_iter: %d\n", options->view.max_iter);
    fprintf(stream, "  duration_ms: %.3f\n", duration_ms);
    fprintf(stream, "  pixels_s: %.3f\n", pixels_s);
    fprintf(stream, "  total_iterations: %llu\n", (unsigned long long)total_iterations);
    fprintf(stream, "  iterations_s: %.3f\n", iterations_s);
}

static void print_json_report(FILE *stream, const BenchOptions *options, double duration_ms, double pixels_s, double iterations_s, uint64_t total_iterations)
{
    fprintf(stream, "{\n");
    fprintf(stream, "  \"project\": \"MandelFarmGigaBrot\",\n");
    fprintf(stream, "  \"bench_version\": \"mandelbench.0.1\",\n");
    fprintf(stream, "  \"backend\": \"scalar_f64\",\n");
    fprintf(stream, "  \"scene\": ");
    print_json_string(stream, options->scene_name);
    fprintf(stream, ",\n");
    fprintf(stream, "  \"width\": %d,\n", options->view.width);
    fprintf(stream, "  \"height\": %d,\n", options->view.height);
    fprintf(stream, "  \"center_re\": %.17g,\n", options->view.center_re);
    fprintf(stream, "  \"center_im\": %.17g,\n", options->view.center_im);
    fprintf(stream, "  \"scale\": %.17g,\n", options->view.scale);
    fprintf(stream, "  \"max_iter\": %d,\n", options->view.max_iter);
    fprintf(stream, "  \"duration_ms\": %.6f,\n", duration_ms);
    fprintf(stream, "  \"pixels_s\": %.6f,\n", pixels_s);
    fprintf(stream, "  \"total_iterations\": %llu,\n", (unsigned long long)total_iterations);
    fprintf(stream, "  \"iterations_s\": %.6f\n", iterations_s);
    fprintf(stream, "}\n");
}

static void print_node_report_json(FILE *stream, const BenchOptions *options, const NodeInfo *node, double duration_ms, double pixels_s, double iterations_s)
{
    fprintf(stream, "{\n");
    fprintf(stream, "  \"project\": \"MandelFarmGigaBrot\",\n");
    fprintf(stream, "  \"protocol\": \"mandelfarm.v1\",\n");
    fprintf(stream, "  \"type\": \"node_capability_report\",\n");
    fprintf(stream, "  \"node\": {\n");
    fprintf(stream, "    \"node_id\": ");
    print_json_string(stream, node->node_id);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"node_name\": ");
    print_json_string(stream, node->node_name);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"os\": ");
    print_json_string(stream, node->os);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"arch\": ");
    print_json_string(stream, node->arch);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"hostname\": ");
    print_json_string(stream, node->hostname);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"logical_cores\": %d\n", node->logical_cores);
    fprintf(stream, "  },\n");
    fprintf(stream, "  \"capabilities\": {\n");
    fprintf(stream, "    \"backends\": [\"scalar_f64\"],\n");
    fprintf(stream, "    \"threading\": false,\n");
    fprintf(stream, "    \"result_formats\": [\"u32_iter\"]\n");
    fprintf(stream, "  },\n");
    fprintf(stream, "  \"benchmark\": {\n");
    fprintf(stream, "    \"bench_version\": \"mandelbench.0.1\",\n");
    fprintf(stream, "    \"backend\": \"scalar_f64\",\n");
    fprintf(stream, "    \"scene\": ");
    print_json_string(stream, options->scene_name);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"width\": %d,\n", options->view.width);
    fprintf(stream, "    \"height\": %d,\n", options->view.height);
    fprintf(stream, "    \"max_iter\": %d,\n", options->view.max_iter);
    fprintf(stream, "    \"duration_ms\": %.6f,\n", duration_ms);
    fprintf(stream, "    \"pixels_s\": %.6f,\n", pixels_s);
    fprintf(stream, "    \"scalar_f64_iter_s\": %.6f\n", iterations_s);
    fprintf(stream, "  }\n");
    fprintf(stream, "}\n");
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

    FILE *report_stream = open_report_stream(&options);

    if (report_stream == 0) {
        free(iterations);
        return 1;
    }

    if (options.node_report) {
        NodeInfo node;

        collect_node_info(&node, options.node_name);
        print_node_report_json(report_stream, &options, &node, duration_ms, pixels_s, iterations_s);
    } else if (options.json) {
        print_json_report(report_stream, &options, duration_ms, pixels_s, iterations_s, total_iterations);
    } else {
        print_human_report(report_stream, &options, duration_ms, pixels_s, iterations_s, total_iterations);
    }

    if (close_report_stream(&options, report_stream) != 0) {
        fprintf(stderr, "failed to close output file\n");
        free(iterations);
        return 1;
    }

    free(iterations);
    return 0;
}
