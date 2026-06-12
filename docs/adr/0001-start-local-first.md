# ADR 0001: Start Local First

## Status

Accepted.

## Context

The long-term vision includes distributed rendering, node discovery, MQTT, heterogeneous workers, and future GPU backends. Starting with those pieces would make it hard to verify correctness and performance.

## Decision

Pilot Zero starts with a local, deterministic C11 Mandelbrot renderer and benchmark. Distributed features remain documented but unimplemented until the local core is correct, measurable, and reusable.

## Consequences

- The math core must not depend on UI, networking, files, or threads.
- The benchmark and renderer can become compatibility anchors for future workers.
- Future agents and orchestrators should consume stable local reports instead of inventing separate measurement paths.

