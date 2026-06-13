# Benchmark Studies

This directory stores repeatable local performance studies for MandelFarmGigaBrot.

The goal is not to chase one perfect number. The goal is to keep enough context
to compare machines, scenes, schedulers, thread counts, and tile sizes without
depending on terminal scrollback.

## Recommended Capture

Build and test first:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Capture a node report:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --json \
  --node-report \
  --node-name "$(hostname)" \
  --threads 10 \
  --scheduler tiles \
  --tile-size 32 \
  --output docs/benchmarks/results/node-report.json
```

Compare fixed row bands:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --thread-sweep 1,2,4,8,10,12,16,20 \
  --scheduler bands \
  --repeat 5 \
  --json \
  --output docs/benchmarks/results/thread-sweep-bands-hard.json
```

Compare dynamic tiles:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --thread-sweep 1,2,4,8,10,12,16,20 \
  --scheduler tiles \
  --tile-size 32 \
  --repeat 5 \
  --json \
  --output docs/benchmarks/results/thread-sweep-tiles32-hard.json
```

Study tile granularity:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --threads 10 \
  --tile-sweep 16,32,64,128,256,512 \
  --repeat 5 \
  --json \
  --output docs/benchmarks/results/tile-sweep-hard-threads10.json
```

Study tile size and thread count together:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --study tile-grid \
  --threads-list 8,10,12,16 \
  --tile-sweep 16,32,64,128 \
  --repeat 5 \
  --json \
  --output docs/benchmarks/results/tile-grid-hard.json
```

## Notes To Record

- Machine name and CPU shape.
- Date and commit.
- Scene, resolution, and max iterations.
- Scheduler and tile size.
- Best, average, and worst times.
- Interpretation: plateau, overhead, load balance, and surprises.
