# MandelFarmGigaBrot

Bring your own CPU. Grow your own Brot.

MandelFarmGigaBrot is an experimental fractal rendering farm for growing very large Mandelbrot images, one tile at a time.

The project starts small: a local C renderer, a benchmark tool, and a simple viewer. The long-term goal is to coordinate heterogeneous devices such as PCs, Macs, Raspberry Pis, phones running Termux, ESP32-class devices, and future CUDA/Vulkan workers into a distributed fractal rendering farm.

## Current Status

Initial project structure, pure C Mandelbrot escape core, viewport type, local buffer renderer, Nanquim demo, `mandel-render` PPM CLI, `mandel-bench` local benchmark, and local node capability reports.

## Planned Binaries

- `mandel-render`: local CLI renderer.
- `mandel-bench`: local benchmark runner.
- `mandel-viewer`: local viewer.
- `mandel-agent`: future node agent.
- `mandel-monitor`: future farm monitor.
- `mandel-compare`: future backend comparison tool.

## Roadmap

- 0.1: local Mandelbrot core and visible demo.
- 0.2: render CLI and benchmark.
- 0.3: local node capability report without networking.
- 0.4: local multicore renderer.
- 0.5: numeric backend comparison.

## Build And Test

From the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The Nanquim demo is built when a local Nanquim checkout is available. By default CMake looks for it at `../Nanquim`; use `NANQUIM_ROOT` if your checkout lives elsewhere:

```sh
cmake -S . -B build -DNANQUIM_ROOT="$HOME/Work/Nanquim"
cmake --build build
./build/bin/mandelbrot_demo
```

Render a PPM image from the command line:

```sh
./build/bin/mandel-render \
  --width 2048 \
  --height 2048 \
  --center-re -0.5 \
  --center-im 0.0 \
  --scale 3.0 \
  --max-iter 1000 \
  --output assets/output/mandel.ppm
```

At this stage, `mandel-render` writes binary PPM P6 files. The Nanquim demo and CLI renderer share the same palette.

Run the local benchmark:

```sh
./build/bin/mandel-bench --scene medium
```

Run a smaller custom benchmark:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --width 1024 \
  --height 768 \
  --max-iter 2000 \
  --threads 4
```

Emit JSON for scripts or future node reports:

```sh
./build/bin/mandel-bench --scene hard --threads 4 --json
```

Available benchmark scenes are `easy`, `medium`, and `hard`. Use `--threads N` to run the row-band pthread renderer. The backend is reported as `scalar_f64` for one thread and `scalar_f64_threads` for multiple threads.

Compare several thread counts in one benchmark run:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --width 1024 \
  --height 768 \
  --max-iter 2000 \
  --thread-sweep 1,2,4,8,10
```

The sweep reports speedup relative to the first thread count in the list.

Generate a local node capability report:

```sh
./build/bin/mandel-bench \
  --scene medium \
  --json \
  --node-report \
  --node-name "$(hostname)" \
  --threads 4 \
  --output assets/output/node-report.json
```

The node report does not publish anything to the network. It records local identity, current scalar capabilities, and benchmark throughput in the future `mandelfarm.v1` shape.

Each completed milestone should include updated run instructions for the binaries and demos available at that point.
