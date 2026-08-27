# Cross-Engine Spatial Data Streaming SDK

An engine-independent C++20 SDK for streaming large tiled spatial datasets
(city/world-scale) into real-time applications — spatial indexing, LOD,
asynchronous priority-driven streaming, and memory-budgeted caching behind
one façade (`spatial::SpatialWorld`), with thin per-engine adapters instead
of per-engine reimplementations of the core.

- [x] Quadtree spatial index (frustum/radius/AABB queries)
- [x] Distance + screen-space-error LOD, with hysteresis
- [x] Asynchronous, priority-driven, cancellable tile streaming
- [x] CPU/GPU memory budgets with priority/recency cache eviction
- [x] Engine-agnostic rendering abstraction + a real D3D11 backend
- [x] Built-in CPU frame profiler (`spatial::debug::Profiler`)
- [x] Standalone viewer, Unity plugin, Unreal plugin, and a custom-engine
      integration — the same core, proven three different ways

```
                            SpatialWorld
                                 │
           ┌───────────────────┼───────────────────┐
           │                   │                   │
      Spatial Index           LOD               Streaming
           │                   │                   │
        Quadtree          SSE + hysteresis    Async workers
                                               │
                                       Priority queue → Tile cache → Memory budgets
                                 │
                                 ▼
                          Rendering (IRenderer)
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
          D3D11 Viewer         Unity      Unreal + Custom (Phoenix) Engine
                                 │
                                 └──────────── Debug / Profiling ──────────┘
```

The core SDK has no dependency on Unity, Unreal, or any specific engine — it
compiles and is fully testable on its own (187 automated tests). Engine
integrations are adapters that implement one interface (`IRenderer`) and
call `SpatialWorld`'s four-method API — see
[docs/case_study.md](docs/case_study.md) for why each integration made a
different call on ABI boundary and coordinate conversion, and what broke
along the way.

![StandaloneViewer streaming a 32x32-tile procedural city, debug tile-bounds overlay enabled](docs/images/standalone_viewer.png)

*`StandaloneViewer` streaming a 1,024-tile procedural city in real time — 69
tiles resident at this camera position, debug overlay (`F1`) showing
color-coded tile bounds. Title bar shows live streaming stats.*

## Why stream instead of just loading the world

Measured, not asserted — see [docs/case_study.md](docs/case_study.md) for
the full methodology:

| Dataset  | Tiles  | Streaming (bounded radius) | Full residency |
|----------|-------:|------------------------------:|------------------------------------:|
| BigCity  |  1,024 |     68 resident / 2.97 MB     | 1,024 resident / 44.7 MB (~12 s) |
| MidCity  |  4,096 |     66 resident / 1.16 MB     | 4,096 resident / 72.1 MB (~16 s) |
| MegaCity | 16,384 |     68 resident / 1.20 MB     | 15,224 resident / 268 MB (not converged after 90 s) |

Growing the dataset 16x barely moves the streaming working set — it's
bounded by streaming radius and memory budget, not by total world size.
The same growth under full residency is roughly linear in memory and worse
than linear in load time.

![StandaloneViewer streaming the 16,384-tile MegaCity dataset, debug tile-bounds overlay enabled, live streaming stats in the title bar](docs/images/megacity_stress_test.png)

*The same viewer and debug overlay, now against the 16,384-tile `MegaCity`
stress-test dataset — 88 tiles resident, ~1.5 MB CPU memory, title bar
showing the live per-section frame-time breakdown from `spatial::debug::Profiler`.*

## Status

**Phase 14 of 14 (portfolio polish) in progress; all 13 engineering phases
complete.** Complete: core spatial math; a binary tile format with
a JSON manifest and a `SpatialTileBuilder` CLI that generates a procedural
city; a generic quadtree spatial index; distance/screen-space-error LOD
selection with hysteresis; a multithreaded streaming manager (request
queue, worker pool, validated resource state machine, priority,
cancellation); a memory-budgeted tile cache with combined priority/recency
eviction; an engine-agnostic rendering abstraction (`IRenderer`, RAII GPU
resources, a bounded upload queue, batched debug-line rendering);
**`StandaloneViewer`** — a Win32 window and Direct3D 11 backend wiring the
above against a generated dataset; **`spatial::SpatialWorld`** — the
single façade class an engine integration owns instead of wiring five
subsystems together itself; a **Unity native plugin**; an **Unreal native
plugin**; and a **custom-engine integration** — validated against an
independent DirectX 12 engine, linking `spatial_sdk` directly rather than
through a C ABI, with no coordinate conversion needed since that engine
shares the SDK's own right-handed/Y-up convention; and a built-in
**CPU frame profiler** (`spatial::debug::Profiler`) that breaks down each
`update()`/`render()` call by streaming, GPU upload, LOD selection, and
debug-draw cost, exposed through `SpatialWorld::frameProfile()` and
`StandaloneViewer`'s `--profile-csv` flag.

187 automated SDK tests, all green. The viewer and both engine plugins are
runnable end to end. See [docs/architecture.md](docs/architecture.md) for
the full design and phased development plan,
[docs/case_study.md](docs/case_study.md) for the design rationale, real
bugs found, and stress-test methodology, and
[CHANGELOG.md](CHANGELOG.md) for what landed in each phase.

glTF/OBJ dataset import is a possible future addition, not yet built —
`SpatialTileBuilder` currently generates procedural datasets only.

## Building

See [docs/getting_started.md](docs/getting_started.md).

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Documentation

- [Architecture](docs/architecture.md)
- [Technical case study](docs/case_study.md)
- [Getting started](docs/getting_started.md)
- [Public SDK API](docs/sdk_api.md)
- [Tile format](docs/tile_format.md)
- [Level of detail](docs/lod.md)
- [Streaming](docs/streaming.md)
- [Rendering](docs/rendering.md)
- [Profiling](docs/profiling.md)
- [Unity integration](docs/unity_integration.md)
- [Unreal integration](docs/unreal_integration.md)
- [Custom engine integration](docs/custom_engine_integration.md)

## License

MIT — see [LICENSE](LICENSE).
