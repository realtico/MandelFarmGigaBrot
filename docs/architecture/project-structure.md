# Project Structure

MandelFarmGigaBrot separates the local renderer from future distributed concerns.

```text
include/             public C headers
src/                 reusable C implementation files
apps/                command-line binaries
examples/            visual demos and experiments
tests/               unit and golden-output tests
docs/                architecture, backlog, and decisions
cmake/               reusable CMake modules
scripts/             developer scripts
assets/              palettes, sample outputs, and visual assets
third_party/         vendored or documented external dependencies
agent/               future node agent work
orchestrator/        future distributed scheduler work
monitor/             future farm monitor work
tools/               local maintenance and analysis tools
```

## Layering

```text
math core
local renderer
benchmark
visualization
tiles
multicore
node agent
orchestrator
distributed cluster
```

The first implemented layer should be a pure C Mandelbrot core with no dependency on rendering windows, files, networking, or concurrency.

