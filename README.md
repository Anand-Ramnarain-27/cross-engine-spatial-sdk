# Cross-Engine Spatial Data Streaming SDK

A portable C++20 SDK for loading, streaming, and rendering large spatial 3D
datasets (city/world-scale) from an engine-independent core, with thin
integration layers for a custom C++ renderer, Unity, and Unreal Engine.

```
        ┌─────────────┐
        │ Spatial SDK │
        └──────┬──────┘
               │
      ┌────────┼────────┐
      │        │        │
    Unity   Unreal   Custom C++
```

The core SDK has no dependency on Unity or Unreal — it compiles and is fully
testable on its own. Engine plugins are adapters that implement a single
rendering interface (`IRenderer`) and call the public API.

## Status

**Phase 8 of 14 — Rendering.** Through Phase 8, the SDK has: core spatial
math and bounding volumes; a binary tile format with a JSON dataset
manifest and a `SpatialTileBuilder` CLI that generates a procedural city;
a generic quadtree spatial index; distance/screen-space-error LOD selection
with hysteresis; a fully multithreaded streaming manager (request queue,
worker pool, validated resource state machine, priority, cancellation);
a memory-budgeted tile cache with combined priority/recency eviction; and
an engine-agnostic rendering abstraction (`IRenderer`, RAII GPU resources,
a bounded upload queue, batched debug-line rendering) — verified against a
mock renderer, no real GPU backend yet. 143 automated tests, all green.
See [docs/architecture.md](docs/architecture.md) for the full design and
phased development plan, and [CHANGELOG.md](CHANGELOG.md) for what landed
in each phase.

Not yet built: a real rendering backend, the standalone viewer, engine
integrations (Unity/Unreal/custom engine), and profiling tooling.

## Planned capabilities

- [x] Spatial tile hierarchy (quadtree) with efficient frustum/distance queries
- [x] Distance-based, then screen-space-error-based, level of detail
- [x] Asynchronous, priority-driven tile streaming with a documented per-tile
      state machine and cancellation
- [x] CPU/GPU memory budgets with combined priority/recency eviction and a tile cache
- [x] An engine-agnostic rendering abstraction (`IRenderer`)
- [ ] A standalone C++ viewer, Unity plugin, Unreal plugin, and custom-engine
      integration, all driven by the same core
- [x] A `SpatialTileBuilder` command-line tool that converts procedural
      geometry into the SDK's dataset + tile format (glTF/OBJ import planned)

## Building

See [docs/getting_started.md](docs/getting_started.md).

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Documentation

- [Architecture](docs/architecture.md)
- [Getting started](docs/getting_started.md)
- [Public SDK API](docs/sdk_api.md)
- [Tile format](docs/tile_format.md)
- [Level of detail](docs/lod.md)
- [Streaming](docs/streaming.md)
- [Rendering](docs/rendering.md)
- [Unity integration](docs/unity_integration.md)
- [Unreal integration](docs/unreal_integration.md)
- [Custom engine integration](docs/custom_engine_integration.md)

## License

MIT — see [LICENSE](LICENSE).
