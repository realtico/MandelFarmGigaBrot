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

#define MAX_THREAD_SWEEP_ITEMS 64
#define MAX_BENCH_REPEAT 1000

typedef enum {
    BENCH_SCHEDULER_BANDS,
    BENCH_SCHEDULER_TILES,
} BenchScheduler;

typedef struct {
    const char *name;
    MandelView view;
} BenchScene;

typedef struct {
    MandelView view;
    const char *scene_name;
    int json;
    int node_report;
    int threads;
    int repeat;
    int tile_size;
    BenchScheduler scheduler;
    int thread_sweep[MAX_THREAD_SWEEP_ITEMS];
    int thread_sweep_count;
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

typedef struct {
    int threads;
    double duration_ms;
    double duration_ms_avg;
    double duration_ms_worst;
    double pixels_s;
    uint64_t total_iterations;
    double iterations_s;
    int repeat;
} BenchmarkResult;

/*
 * Benchmark scenes are canned workloads. They are useful because performance
 * discussions need stable inputs: changing the view can change the amount of
 * Mandelbrot work per pixel dramatically.
 */
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
        "          [--center-re X --center-im Y --scale X] [--threads N] [--repeat N]\n"
        "          [--scheduler bands|tiles] [--tile-size N]\n"
        "          [--thread-sweep 1,2,4,8]\n"
        "          [--node-report] [--node-name NAME] [--output FILE]\n",
        program);
}

static int parse_int_arg(const char *value, int *out)
{
    char *end = 0;
    long parsed = 0;

    /*
     * Reject partial parses such as "8threads". Benchmarks are only useful when
     * the recorded configuration is exactly what the user requested.
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

    /* Coordinates and scale are fractional values in the complex plane. */
    errno = 0;
    parsed = strtod(value, &end);

    if (errno != 0 || end == value || *end != '\0') {
        return -1;
    }

    *out = parsed;
    return 0;
}

static int parse_thread_sweep_arg(const char *value, BenchOptions *options)
{
    const char *cursor = value;
    int count = 0;

    /*
     * --thread-sweep is a compact comma-separated list because it is commonly
     * typed by hand while exploring scaling curves: 1,2,4,8,10,12,16...
     */
    while (*cursor != '\0') {
        char *end = 0;
        long parsed = 0;

        errno = 0;
        parsed = strtol(cursor, &end, 10);

        if (errno != 0 || end == cursor || parsed <= 0 || parsed > 2147483647L) {
            return -1;
        }

        if (count >= MAX_THREAD_SWEEP_ITEMS) {
            fprintf(stderr, "thread sweep supports at most %d entries\n", MAX_THREAD_SWEEP_ITEMS);
            return -1;
        }

        options->thread_sweep[count++] = (int)parsed;

        if (*end == ',') {
            cursor = end + 1;
            if (*cursor == '\0') {
                return -1;
            }
        } else if (*end == '\0') {
            cursor = end;
        } else {
            return -1;
        }
    }

    if (count == 0) {
        return -1;
    }

    options->thread_sweep_count = count;
    return 0;
}

static int parse_scheduler_arg(const char *value, BenchScheduler *scheduler)
{
    if (strcmp(value, "bands") == 0) {
        *scheduler = BENCH_SCHEDULER_BANDS;
        return 0;
    }

    if (strcmp(value, "tiles") == 0) {
        *scheduler = BENCH_SCHEDULER_TILES;
        return 0;
    }

    return -1;
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
    /*
     * Parse --scene first so later dimension flags can override the selected
     * preset. Example: --scene hard --width 512 keeps the hard center/scale but
     * makes the benchmark smaller.
     */
    if (apply_scene(options, "medium") != 0) {
        return -1;
    }
    options->json = 0;
    options->node_report = 0;
    options->threads = 1;
    options->repeat = 1;
    options->tile_size = 128;
    options->scheduler = BENCH_SCHEDULER_BANDS;
    options->thread_sweep_count = 0;
    options->node_name = 0;
    options->output_path = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scene") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || apply_scene(options, argv[++i]) != 0) {
                return -1;
            }
        }
    }

    /*
     * Second pass applies all other flags. We deliberately skip --scene here
     * because it was already consumed in the first pass.
     */
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
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->threads) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--repeat") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->repeat) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--scheduler") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_scheduler_arg(argv[++i], &options->scheduler) != 0) {
                fprintf(stderr, "--scheduler expects bands or tiles\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--tile-size") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_int_arg(argv[++i], &options->tile_size) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--thread-sweep") == 0) {
            if (require_value(i, argc, argv[i]) != 0 || parse_thread_sweep_arg(argv[++i], options) != 0) {
                fprintf(stderr, "--thread-sweep expects a comma-separated list of positive integers, e.g. 1,2,4,8\n");
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

    if (options->repeat <= 0 || options->repeat > MAX_BENCH_REPEAT) {
        fprintf(stderr, "repeat must be between 1 and %d\n", MAX_BENCH_REPEAT);
        return -1;
    }

    if (options->threads <= 0) {
        fprintf(stderr, "threads must be positive\n");
        return -1;
    }

    if (options->tile_size <= 0) {
        fprintf(stderr, "tile-size must be positive\n");
        return -1;
    }

    if (options->thread_sweep_count > 0) {
        /*
         * The first sweep entry is the baseline shown in shared fields such as
         * bench_backend_name(). Individual rows still keep their own thread
         * count.
         */
        options->threads = options->thread_sweep[0];
    }

    if (options->node_report && !options->json) {
        fprintf(stderr, "--node-report currently requires --json\n");
        return -1;
    }

    if (options->node_report && options->thread_sweep_count > 0) {
        fprintf(stderr, "--node-report cannot be combined with --thread-sweep yet\n");
        return -1;
    }

    return 0;
}

static double monotonic_seconds(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;

    /*
     * Wall-clock time can jump if the system clock changes. A monotonic clock
     * only moves forward, which is what a benchmark duration needs.
     */
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

    /*
     * The sum is not used to render the image; it is a workload metric. Two
     * views with the same resolution can have very different total iteration
     * counts depending on how much of the set they touch.
     */
    for (size_t i = 0; i < count; ++i) {
        total += iterations[i];
    }

    return total;
}

static const char *backend_name_for_threads(int threads)
{
    return threads == 1 ? "scalar_f64" : "scalar_f64_threads";
}

static const char *scheduler_name(BenchScheduler scheduler)
{
    return scheduler == BENCH_SCHEDULER_TILES ? "tiles" : "bands";
}

static const char *backend_name_for_config(int threads, BenchScheduler scheduler)
{
    if (scheduler == BENCH_SCHEDULER_TILES) {
        return "scalar_f64_tile_queue";
    }

    return backend_name_for_threads(threads);
}

static const char *bench_backend_name(const BenchOptions *options)
{
    return backend_name_for_config(options->threads, options->scheduler);
}

static uint32_t fnv1a_update(uint32_t hash, const char *text)
{
    /*
     * Tiny non-cryptographic hash used only to make local node IDs less likely
     * to collide. This is not a security boundary.
     */
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

    /*
     * Normalize names into a simple identifier shape so future reports are easy
     * to compare in scripts and logs.
     */
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

    /*
     * We do not pull a JSON library into this tiny C tool yet, so strings need
     * explicit escaping before they are written into reports.
     */
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

    /*
     * Node reports are local capability snapshots. They are not sent anywhere
     * yet, but this shape prepares the future farm agent protocol.
     */
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
    /*
     * All report printers write to FILE*. That keeps stdout and --output using
     * exactly the same formatting code.
     */
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

static int run_benchmark_once(
    const MandelView *view,
    int threads,
    BenchScheduler scheduler,
    int tile_size,
    uint32_t *iterations,
    size_t pixel_count,
    BenchmarkResult *result)
{
    /*
     * This measures the render call only. Allocation and report formatting stay
     * outside the timed section so the number reflects the backend itself.
     */
    const double start_s = monotonic_seconds();
    const int render_result = scheduler == BENCH_SCHEDULER_TILES
        ? mandel_render_f64_tile_threads(view, iterations, threads, tile_size)
        : (threads == 1 ? mandel_render_f64(view, iterations) : mandel_render_f64_threads(view, iterations, threads));
    const double end_s = monotonic_seconds();

    if (render_result != 0 || end_s < start_s) {
        return -1;
    }

    const double duration_s = end_s - start_s;

    result->threads = threads;
    result->duration_ms = duration_s * 1000.0;
    result->duration_ms_avg = result->duration_ms;
    result->duration_ms_worst = result->duration_ms;
    result->total_iterations = sum_iterations(iterations, pixel_count);
    result->pixels_s = duration_s > 0.0 ? (double)pixel_count / duration_s : 0.0;
    result->iterations_s = duration_s > 0.0 ? (double)result->total_iterations / duration_s : 0.0;
    result->repeat = 1;
    return 0;
}

static int run_benchmark_repeated(
    const MandelView *view,
    int threads,
    BenchScheduler scheduler,
    int tile_size,
    int repeat,
    uint32_t *iterations,
    size_t pixel_count,
    BenchmarkResult *result)
{
    BenchmarkResult best;
    double duration_sum = 0.0;
    double duration_worst = 0.0;

    /*
     * Microbenchmarks are noisy: the OS scheduler, background processes and
     * cache state can move a single measurement around. We therefore run the
     * same configuration several times and keep:
     *
     * - duration_ms: the best observed time, useful for comparing the engine;
     * - duration_ms_avg: the average, useful for seeing typical behavior;
     * - duration_ms_worst: the slowest run, useful for spotting jitter.
     */
    for (int i = 0; i < repeat; ++i) {
        BenchmarkResult current;

        if (run_benchmark_once(view, threads, scheduler, tile_size, iterations, pixel_count, &current) != 0) {
            return -1;
        }

        if (i == 0 || current.duration_ms < best.duration_ms) {
            best = current;
        }

        if (i == 0 || current.duration_ms > duration_worst) {
            duration_worst = current.duration_ms;
        }

        duration_sum += current.duration_ms;
    }

    *result = best;
    result->duration_ms_avg = duration_sum / (double)repeat;
    result->duration_ms_worst = duration_worst;
    result->repeat = repeat;
    return 0;
}

static void print_human_report(FILE *stream, const BenchOptions *options, const BenchmarkResult *result)
{
    fprintf(stream, "MandelFarmGigaBrot benchmark\n");
    fprintf(stream, "  bench_version: mandelbench.0.1\n");
    fprintf(stream, "  backend: %s\n", bench_backend_name(options));
    fprintf(stream, "  scheduler: %s\n", scheduler_name(options->scheduler));
    if (options->scheduler == BENCH_SCHEDULER_TILES) {
        fprintf(stream, "  tile_size: %d\n", options->tile_size);
    }
    fprintf(stream, "  threads: %d\n", options->threads);
    fprintf(stream, "  repeat: %d\n", result->repeat);
    fprintf(stream, "  scene: %s\n", options->scene_name);
    fprintf(stream, "  resolution: %dx%d\n", options->view.width, options->view.height);
    fprintf(stream, "  center: %.15g, %.15g\n", options->view.center_re, options->view.center_im);
    fprintf(stream, "  scale: %.15g\n", options->view.scale);
    fprintf(stream, "  max_iter: %d\n", options->view.max_iter);
    fprintf(stream, "  duration_ms: %.3f\n", result->duration_ms);
    fprintf(stream, "  duration_ms_avg: %.3f\n", result->duration_ms_avg);
    fprintf(stream, "  duration_ms_worst: %.3f\n", result->duration_ms_worst);
    fprintf(stream, "  pixels_s: %.3f\n", result->pixels_s);
    fprintf(stream, "  total_iterations: %llu\n", (unsigned long long)result->total_iterations);
    fprintf(stream, "  iterations_s: %.3f\n", result->iterations_s);
}

static void print_thread_sweep_human(FILE *stream, const BenchOptions *options, const BenchmarkResult *results, int result_count)
{
    const double baseline = result_count > 0 ? results[0].iterations_s : 0.0;

    /*
     * Speedup is relative to the first sweep entry, not necessarily to one
     * thread. That lets users compare any sequence they choose.
     */
    fprintf(stream, "MandelFarmGigaBrot thread sweep\n");
    fprintf(stream, "  bench_version: mandelbench.0.1\n");
    fprintf(stream, "  scene: %s\n", options->scene_name);
    fprintf(stream, "  resolution: %dx%d\n", options->view.width, options->view.height);
    fprintf(stream, "  max_iter: %d\n", options->view.max_iter);
    fprintf(stream, "  scheduler: %s\n", scheduler_name(options->scheduler));
    if (options->scheduler == BENCH_SCHEDULER_TILES) {
        fprintf(stream, "  tile_size: %d\n", options->tile_size);
    }
    fprintf(stream, "  repeat: %d\n", options->repeat);
    fprintf(stream, "\n");
    fprintf(stream, "%8s  %12s  %12s  %12s  %16s  %8s\n", "Threads", "Best(ms)", "Avg(ms)", "Worst(ms)", "Iter/s", "Speedup");

    for (int i = 0; i < result_count; ++i) {
        const double speedup = baseline > 0.0 ? results[i].iterations_s / baseline : 0.0;

        fprintf(stream, "%8d  %12.3f  %12.3f  %12.3f  %16.3f  %7.2fx\n",
            results[i].threads,
            results[i].duration_ms,
            results[i].duration_ms_avg,
            results[i].duration_ms_worst,
            results[i].iterations_s,
            speedup);
    }
}

static void print_json_report(FILE *stream, const BenchOptions *options, const BenchmarkResult *result)
{
    fprintf(stream, "{\n");
    fprintf(stream, "  \"project\": \"MandelFarmGigaBrot\",\n");
    fprintf(stream, "  \"bench_version\": \"mandelbench.0.1\",\n");
    fprintf(stream, "  \"backend\": ");
    print_json_string(stream, bench_backend_name(options));
    fprintf(stream, ",\n");
    fprintf(stream, "  \"scheduler\": ");
    print_json_string(stream, scheduler_name(options->scheduler));
    fprintf(stream, ",\n");
    fprintf(stream, "  \"tile_size\": %d,\n", options->tile_size);
    fprintf(stream, "  \"threads\": %d,\n", options->threads);
    fprintf(stream, "  \"repeat\": %d,\n", result->repeat);
    fprintf(stream, "  \"scene\": ");
    print_json_string(stream, options->scene_name);
    fprintf(stream, ",\n");
    fprintf(stream, "  \"width\": %d,\n", options->view.width);
    fprintf(stream, "  \"height\": %d,\n", options->view.height);
    fprintf(stream, "  \"center_re\": %.17g,\n", options->view.center_re);
    fprintf(stream, "  \"center_im\": %.17g,\n", options->view.center_im);
    fprintf(stream, "  \"scale\": %.17g,\n", options->view.scale);
    fprintf(stream, "  \"max_iter\": %d,\n", options->view.max_iter);
    fprintf(stream, "  \"duration_ms\": %.6f,\n", result->duration_ms);
    fprintf(stream, "  \"duration_ms_best\": %.6f,\n", result->duration_ms);
    fprintf(stream, "  \"duration_ms_avg\": %.6f,\n", result->duration_ms_avg);
    fprintf(stream, "  \"duration_ms_worst\": %.6f,\n", result->duration_ms_worst);
    fprintf(stream, "  \"pixels_s\": %.6f,\n", result->pixels_s);
    fprintf(stream, "  \"total_iterations\": %llu,\n", (unsigned long long)result->total_iterations);
    fprintf(stream, "  \"iterations_s\": %.6f\n", result->iterations_s);
    fprintf(stream, "}\n");
}

static void print_thread_sweep_json(FILE *stream, const BenchOptions *options, const BenchmarkResult *results, int result_count)
{
    const double baseline = result_count > 0 ? results[0].iterations_s : 0.0;

    fprintf(stream, "{\n");
    fprintf(stream, "  \"project\": \"MandelFarmGigaBrot\",\n");
    fprintf(stream, "  \"bench_version\": \"mandelbench.0.1\",\n");
    fprintf(stream, "  \"type\": \"thread_sweep\",\n");
    fprintf(stream, "  \"scene\": ");
    print_json_string(stream, options->scene_name);
    fprintf(stream, ",\n");
    fprintf(stream, "  \"width\": %d,\n", options->view.width);
    fprintf(stream, "  \"height\": %d,\n", options->view.height);
    fprintf(stream, "  \"max_iter\": %d,\n", options->view.max_iter);
    fprintf(stream, "  \"scheduler\": ");
    print_json_string(stream, scheduler_name(options->scheduler));
    fprintf(stream, ",\n");
    fprintf(stream, "  \"tile_size\": %d,\n", options->tile_size);
    fprintf(stream, "  \"repeat\": %d,\n", options->repeat);
    fprintf(stream, "  \"results\": [\n");

    for (int i = 0; i < result_count; ++i) {
        const double speedup = baseline > 0.0 ? results[i].iterations_s / baseline : 0.0;

        fprintf(stream, "    {\n");
        fprintf(stream, "      \"backend\": ");
        print_json_string(stream, backend_name_for_config(results[i].threads, options->scheduler));
        fprintf(stream, ",\n");
        fprintf(stream, "      \"scheduler\": ");
        print_json_string(stream, scheduler_name(options->scheduler));
        fprintf(stream, ",\n");
        fprintf(stream, "      \"tile_size\": %d,\n", options->tile_size);
        fprintf(stream, "      \"threads\": %d,\n", results[i].threads);
        fprintf(stream, "      \"repeat\": %d,\n", results[i].repeat);
        fprintf(stream, "      \"duration_ms\": %.6f,\n", results[i].duration_ms);
        fprintf(stream, "      \"duration_ms_best\": %.6f,\n", results[i].duration_ms);
        fprintf(stream, "      \"duration_ms_avg\": %.6f,\n", results[i].duration_ms_avg);
        fprintf(stream, "      \"duration_ms_worst\": %.6f,\n", results[i].duration_ms_worst);
        fprintf(stream, "      \"pixels_s\": %.6f,\n", results[i].pixels_s);
        fprintf(stream, "      \"total_iterations\": %llu,\n", (unsigned long long)results[i].total_iterations);
        fprintf(stream, "      \"iterations_s\": %.6f,\n", results[i].iterations_s);
        fprintf(stream, "      \"speedup\": %.6f\n", speedup);
        fprintf(stream, "    }%s\n", i + 1 == result_count ? "" : ",");
    }

    fprintf(stream, "  ]\n");
    fprintf(stream, "}\n");
}

static void print_node_report_json(FILE *stream, const BenchOptions *options, const NodeInfo *node, const BenchmarkResult *result)
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
    fprintf(stream, "    \"backends\": [\"scalar_f64\", \"scalar_f64_threads\", \"scalar_f64_tile_queue\"],\n");
    fprintf(stream, "    \"threading\": true,\n");
    fprintf(stream, "    \"result_formats\": [\"u32_iter\"]\n");
    fprintf(stream, "  },\n");
    fprintf(stream, "  \"benchmark\": {\n");
    fprintf(stream, "    \"bench_version\": \"mandelbench.0.1\",\n");
    fprintf(stream, "    \"backend\": ");
    print_json_string(stream, bench_backend_name(options));
    fprintf(stream, ",\n");
    fprintf(stream, "    \"scheduler\": ");
    print_json_string(stream, scheduler_name(options->scheduler));
    fprintf(stream, ",\n");
    fprintf(stream, "    \"tile_size\": %d,\n", options->tile_size);
    fprintf(stream, "    \"threads\": %d,\n", options->threads);
    fprintf(stream, "    \"repeat\": %d,\n", result->repeat);
    fprintf(stream, "    \"scene\": ");
    print_json_string(stream, options->scene_name);
    fprintf(stream, ",\n");
    fprintf(stream, "    \"width\": %d,\n", options->view.width);
    fprintf(stream, "    \"height\": %d,\n", options->view.height);
    fprintf(stream, "    \"max_iter\": %d,\n", options->view.max_iter);
    fprintf(stream, "    \"duration_ms\": %.6f,\n", result->duration_ms);
    fprintf(stream, "    \"duration_ms_best\": %.6f,\n", result->duration_ms);
    fprintf(stream, "    \"duration_ms_avg\": %.6f,\n", result->duration_ms_avg);
    fprintf(stream, "    \"duration_ms_worst\": %.6f,\n", result->duration_ms_worst);
    fprintf(stream, "    \"pixels_s\": %.6f,\n", result->pixels_s);
    fprintf(stream, "    \"scalar_f64_iter_s\": %.6f\n", result->iterations_s);
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

    FILE *report_stream = open_report_stream(&options);

    if (report_stream == 0) {
        free(iterations);
        return 1;
    }

    if (options.thread_sweep_count > 0) {
        BenchmarkResult results[MAX_THREAD_SWEEP_ITEMS];

        /*
         * Each sweep entry renders the same scene into the same caller-owned
         * buffer. The previous contents do not matter because each run writes
         * the full image before metrics are collected.
         *
         * Conceptually this answers: "for this same problem, how does changing
         * only the thread count affect throughput?"
         */
        for (int i = 0; i < options.thread_sweep_count; ++i) {
            if (run_benchmark_repeated(
                    &options.view,
                    options.thread_sweep[i],
                    options.scheduler,
                    options.tile_size,
                    options.repeat,
                    iterations,
                    pixel_count,
                    &results[i]) != 0) {
                fprintf(stderr, "failed to benchmark Mandelbrot render for %d threads\n", options.thread_sweep[i]);
                close_report_stream(&options, report_stream);
                free(iterations);
                return 1;
            }
        }

        if (options.json) {
            print_thread_sweep_json(report_stream, &options, results, options.thread_sweep_count);
        } else {
            print_thread_sweep_human(report_stream, &options, results, options.thread_sweep_count);
        }
    } else {
        BenchmarkResult result;

        /*
         * Non-sweep mode measures one backend configuration. It is the path used
         * by plain human output, JSON output, and the local node capability
         * report.
         */
        if (run_benchmark_repeated(
                &options.view,
                options.threads,
                options.scheduler,
                options.tile_size,
                options.repeat,
                iterations,
                pixel_count,
                &result) != 0) {
            fprintf(stderr, "failed to benchmark Mandelbrot render\n");
            close_report_stream(&options, report_stream);
            free(iterations);
            return 1;
        }

        if (options.node_report) {
            NodeInfo node;

            collect_node_info(&node, options.node_name);
            print_node_report_json(report_stream, &options, &node, &result);
        } else if (options.json) {
            print_json_report(report_stream, &options, &result);
        } else {
            print_human_report(report_stream, &options, &result);
        }
    }

    if (close_report_stream(&options, report_stream) != 0) {
        fprintf(stderr, "failed to close output file\n");
        free(iterations);
        return 1;
    }

    free(iterations);
    return 0;
}
