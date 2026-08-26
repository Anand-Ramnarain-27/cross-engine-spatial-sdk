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

**Phase 9 of 14 — Standalone Viewer.** The SDK now has a complete,
runnable demonstration: core spatial math; a binary tile format with a
JSON manifest and a `SpatialTileBuilder` CLI that generates a procedural
city; a generic quadtree spatial index; distance/screen-space-error LOD
selection with hysteresis; a fully multithreaded streaming manager
(request queue, worker pool, validated resource state machine, priority,
cancellation); a memory-budgeted tile cache with combined priority/recency
eviction; an engine-agnostic rendering abstraction (`IRenderer`, RAII GPU
resources, a bounded upload queue, batched debug-line rendering); and
**`StandaloneViewer`** — a real Win32 window, a real Direct3D 11 backend,
and all of the above wired together against a real generated dataset. 143
automated SDK tests, all green; the viewer itself is verified by actually
running it (see `docs/architecture.md`'s Phase 9 section). See
[docs/architecture.md](docs/architecture.md) for the full design and
phased development plan, and [CHANGELOG.md](CHANGELOG.md) for what landed
in each phase.

Not yet built: Unity/Unreal/custom-engine integrations, and profiling
tooling.

## Planned capabilities

- [x] Spatial tile hierarchy (quadtree) with efficient frustum/distance queries
- [x] Distance-based, then screen-space-error-based, level of detail
- [x] Asynchronous, priority-driven tile streaming with a documented per-tile
      state machine and cancellation
- [x] CPU/GPU memory budgets with combined priority/recency eviction and a tile cache
- [x] An engine-agnostic rendering abstraction (`IRenderer`) with a real
      Direct3D 11 backend
- [x] A standalone C++ viewer (`StandaloneViewer`) — free-fly camera,
      streamed world, LOD, debug visualization, runtime stats
- [ ] Unity plugin, Unreal plugin, and custom-engine integration, all
      driven by the same core
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
