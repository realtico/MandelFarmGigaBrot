# MandelFarmGigaBrot

Bring your own CPU. Grow your own Brot.

MandelFarmGigaBrot is an experimental fractal rendering farm for growing very large Mandelbrot images, one tile at a time.

The project starts small: a local C renderer, a benchmark tool, and a simple viewer. The long-term goal is to coordinate heterogeneous devices such as PCs, Macs, Raspberry Pis, phones running Termux, ESP32-class devices, and future CUDA/Vulkan workers into a distributed fractal rendering farm.

## Current Status

Initial project structure only. No Mandelbrot implementation has been added yet.

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

