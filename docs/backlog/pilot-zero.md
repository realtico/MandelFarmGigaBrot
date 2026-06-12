# Pilot Zero Backlog

The Pilot Zero goal is to create the smallest useful version of MandelFarmGigaBrot:

- local Mandelbrot renderer;
- single-core first;
- adjustable resolution;
- local benchmark;
- optional Nanquim visualization;
- structure ready for multicore rendering.

## Milestones

### 0.1 - Visible Local Mandelbrot

- P0-001: pure C Mandelbrot math core.
- P0-002: viewport structure.
- P0-003: local buffer renderer.
- P1-001: Nanquim demo.
- P1-002: CMake integration for the demo.

### 0.2 - CLI Render And Benchmark

- P2-001: `mandel-render`.
- P2-002: simple shared palette.
- P3-001: `mandel-bench`.
- P3-002: JSON benchmark output.
- P3-003: fixed benchmark scenes.

### 0.3 - Node Report Without Network

- P4-001: node identity structure.
- P4-002: local capability report.
- P8-001: initial README.

### 0.4 - Local Multicore

- P5-001: multithread render by row bands.
- P5-002: thread sweep benchmark.
- P5-003: local tile rendering.

### 0.5 - Numeric Backends

- P6-001: `scalar_f32`.
- P6-002: `mandel-compare`.
- P6-003: fixed-point backend planning.

