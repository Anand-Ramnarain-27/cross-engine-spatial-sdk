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

**Phase 1 — Project Foundation.** CMake project, SDK library skeleton,
Catch2 test suite, and CI are in place. No spatial, streaming, or rendering
logic exists yet; see [docs/architecture.md](docs/architecture.md) for the
target design and the phased development plan.

## Planned capabilities

- Spatial tile hierarchy (quadtree) with efficient frustum/distance queries
- Distance-based, then screen-space-error-based, level of detail
- Asynchronous, priority-driven tile streaming with a documented per-tile
  state machine and cancellation
- CPU/GPU memory budgets with LRU/priority-based eviction and a tile cache
- An engine-agnostic rendering abstraction (`IRenderer`)
- A standalone C++ viewer, Unity plugin, Unreal plugin, and custom-engine
  integration, all driven by the same core
- A `SpatialTileBuilder` command-line tool that converts glTF/OBJ/procedural
  geometry into the SDK's dataset + tile format

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
- [Streaming](docs/streaming.md)
- [Level of detail](docs/lod.md)
- [Tile format](docs/tile_format.md)
- [Unity integration](docs/unity_integration.md)
- [Unreal integration](docs/unreal_integration.md)
- [Custom engine integration](docs/custom_engine_integration.md)

## License

MIT — see [LICENSE](LICENSE).
