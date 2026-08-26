# Architecture

## Goals

The Cross-Engine Spatial Data Streaming SDK loads, streams, and renders large
spatial 3D datasets without depending on any particular game engine. A single
engine-independent C++ core is shared by a standalone renderer, a Unity
plugin, an Unreal plugin, and a custom C++ engine integration.

## Layering

```
                    Spatial SDK
                        |
              +---------+---------+
              |                   |
          Core SDK            Rendering API
              |                   |
       +------+------+      +-----+------+
       |             |      |            |
 Spatial Data    Streaming  GPU Abstraction
       |
 +-----+------+--------+
 |            |        |
Tiles       LOD     Queries
 |
Serialization
```

| Layer | Depends on | Must NOT depend on |
|---|---|---|
| Core | nothing | Data, Streaming, Rendering, any engine |
| Data | Core | Streaming, Rendering, any engine |
| LOD / Culling | Core | Streaming, Rendering, any engine |
| Streaming | Core, Data, LOD, Culling | Rendering, any engine |
| Rendering (`IRenderer`) | Core (types only) | any concrete engine |
| API | Core, Data, Streaming, Rendering (interface only) | any concrete engine |
| Engine integrations (Unity/Unreal/Custom) | API, `IRenderer` | — (they are the leaves) |

This is enforced structurally: engine-specific code lives entirely under
`examples/`, never under `sdk/`. `sdk/` must compile with no Unity or Unreal
installation present.

## Module map

```
SpatialSDK
│
├── Core            math, bounding volumes, spatial index, coordinate systems
├── Data             tile/dataset model, binary tile format, serialization
├── Streaming        streaming manager, request queue, worker threads, cache,
│                     memory budget, per-tile resource state machine
├── LOD              LOD selection (distance, then screen-space error), hysteresis
├── Culling          frustum culling, distance culling, visibility
├── Rendering        IRenderer abstraction, GPU upload queue, resource handles
├── Debug            debug renderer, statistics, profiling instrumentation
└── API              SpatialWorld / SpatialDataset / Camera public facade
```

## Data flow (per frame)

```
camera position/frustum
        |
        v
StreamingManager.update()
        |
        +--> SpatialIndex query (frustum + distance)   [Culling]
        +--> LODManager.select(tile, camera)            [LOD]
        |
        v
   desired tile set  vs.  resident tile set
        |
        v
   diff -> load requests (Requested) / unload requests (UnloadRequested)
        |
        v
   RequestQueue (priority) -> WorkerThreads
        |                         |
        |                  file IO, decompress, deserialize
        |                         v
        |                    CPU Tile (LoadedCPU)
        |                         |
        v                         v
   GPUUploadQueue  <---------------
        |
        v
   IRenderer creates GPU resources -> Resident
```

## State machine

Every tile's resource state is one of:

```
Unloaded -> Requested -> Loading -> LoadedCPU -> UploadPending -> Resident -> UnloadRequested -> Unloading -> Unloaded
```

Transitions are only ever performed by `StreamingManager`; no other system
mutates tile state directly, and invalid transitions (e.g. `Unloaded ->
Resident`) are rejected. This is documented in detail in
[streaming.md](streaming.md) once Phase 6 lands.

## Why these boundaries

- **`IRenderer` instead of a concrete renderer**: lets the identical
  Streaming/LOD/Culling code drive a custom D3D/Vulkan renderer, Unity's
  render pipeline, or Unreal's RHI, without recompiling the core for any of
  them.
- **Core has zero dependencies**: it is the part most reused/tested and least
  likely to change; keeping it dependency-free means it can be unit tested in
  isolation and never breaks when a streaming or rendering decision changes.
- **Streaming does not know about GPU resources directly**: it only enqueues
  upload requests and reacts to completion; this keeps GPU-thread-affinity
  concerns (a D3D/Vulkan/GL context usually can't be touched from a worker
  thread) out of the streaming logic entirely.

## Core math conventions (Phase 2)

`spatial::core` (`sdk/include/spatial/core/`) implements the SDK's vector,
matrix, and bounding-volume types. These are pure, header-only, and have no
dependency on anything outside `<cmath>`/`<array>`/etc., so they can be unit
tested in complete isolation from streaming or rendering.

- `Vec2`, `Vec3`, `Vec4` — value types, no dynamic allocation.
- `Mat4` — logically row-major storage (`m[row][col]`), **column-vector**
  convention (`v' = M * v`), right-handed, perspective projection targets an
  OpenGL-style NDC depth range of `[-1, 1]`. This is an internal SDK
  convention, not any particular engine's — Unity/Unreal/custom-engine
  integration layers convert to/from their own matrix conventions at the
  `IRenderer`/`Camera` boundary, which is exactly where that conversion
  belongs.
- `Plane` — normal-distance form (`normal . p + distance = 0`); "positive
  side" is caller-defined (for `Frustum`, positive = inside).
- `AABB`, `Sphere` — the two bounding volumes used everywhere else in the
  SDK (tile bounds, spatial index nodes, culling tests).
- `Frustum` — six inward-facing planes extracted from a combined
  view-projection matrix via the standard Gribb/Hartmann method;
  `intersectsAABB`/`intersectsSphere` are conservative (a straddling volume
  counts as visible), which is the correct behavior for culling.
- `CoordinateSystem` — currently a single value (`LocalCartesian`), matching
  the dataset manifest's `"coordinateSystem"` field; modeled as an enum
  rather than a string so future systems are a compile-time-checked addition.

## Data model and serialization (Phase 3)

`spatial::data` (`sdk/include/spatial/data/`) is the tile/dataset model and
its two on-disk formats — the JSON dataset manifest and the binary `.tile`
format. Full schema in [tile_format.md](tile_format.md); summary here:

- `TileId{level, x, y}` — quadtree address; parent/child addresses are
  computed, not looked up.
- `Tile` — id, `AABB` bounds, optional parent, child ids, a list of
  `TileLOD`, a list of `Material`, and free-form `Metadata`. Pure data: no
  streaming state, no GPU handles.
- `TileLOD` — a `geometricError` plus one `Mesh` per material used at that
  LOD (a tile is rarely one material, e.g. ground vs. buildings).
- `TileSerializer::loadTile`/`saveTile` — binary format, `Expected<Tile>`/
  `Expected<void>`.
- `DatasetManifest` + `DatasetSerializer::loadManifest`/`saveManifest` — the
  JSON manifest. `nlohmann::json` is used only inside
  `DatasetSerializer.cpp` and linked `PRIVATE`, so it never appears in a
  public header or a consumer's include path.
- `spatial::Expected<T>`/`Error` (`sdk/include/spatial/Error.h`) — introduced
  in this phase because serialization is the first place failure is a normal
  outcome (missing file, corrupt data, version mismatch). A minimal
  dependency-free stand-in for C++23's `std::expected`, used instead of
  exceptions specifically because exceptions are unsuitable for crossing an
  engine/plugin ABI boundary (see docs/sdk_api.md once Phase 9+ needs it).

**`SpatialTileBuilder`** (`tools/TileBuilder/`) is the CLI that produces a
dataset: a procedural city generator (ground + a grid of box buildings per
tile, deterministic per `(seed, tileId)`) writes a `.world` manifest and one
`.tile` file per grid cell. Procedural generation was chosen over a glTF/OBJ
importer for this phase — a robust mesh importer is a large, separate effort
disproportionate to what this SDK is actually demonstrating (streaming, not
asset-pipeline robustness), and procedural generation directly produces the
Phase 25 stress-test dataset for free. A glTF/OBJ import path can be added
later as an additional input without changing the tile format.

## Spatial hierarchy (Phase 4)

`core::SpatialIndex<T>` (`sdk/include/spatial/core/SpatialIndex.h`) is a
generic quadtree over axis-aligned bounds — it stores `{T, AABB}` pairs and
has no knowledge of tiles, keeping `spatial::core` dependency-free per the
layering rule above. It splits X/Z only (a true "quad" tree); an item is
stored at the smallest node whose region fully contains its bounds, which is
what makes pruning by node region correct during a query. Supports
`queryFrustum`, `queryRadius`, and `queryAABB`.

`data::TileIndex` (`sdk/include/spatial/data/TileIndex.h`) is the
tile-specific façade: a `SpatialIndex<TileId>` plus an `unordered_map` for
O(1) lookup by id. It indexes tile *existence and bounds*, not content —
`TileIndex::buildUniformGrid` computes every tile's bounds directly from the
dataset manifest's `tileSize`/`worldSize` (the same arithmetic
`SpatialTileBuilder` uses to lay out the grid), so building the index never
reads a `.tile` file. This is deliberate: figuring out which tiles are
spatially relevant has to be cheap and instant, independent of how expensive
loading their content is — that separation is exactly what makes streaming
possible later.

Note `TileId.level` (quadtree address depth, used for tile file naming) and
a tile's LOD count (`Tile::lods()`, geometric detail) are different axes.
`SpatialTileBuilder` currently only populates one `TileId.level`; deeper/
shallower levels are addressable but unused until a future phase needs a
multi-resolution tile hierarchy.

**Benchmarks** (`tests/core/SpatialIndexBenchmark.cpp`, Catch2 `BENCHMARK`,
excluded from the default `ctest` run via the hidden `[.]` tag — run with
`spatial_sdk_tests.exe "[spatialindex][benchmark]"`), 10,000 items scattered
across a 40,000 x 40,000 unit world, Debug build:

| Query | Brute force (mean) | `SpatialIndex` (mean) | Speedup |
|---|---|---|---|
| Frustum (camera sees a local neighborhood) | 456.6 us | 15.7 us | ~29x |
| Radius (200 units) | 456.1 us | 1.9 us | ~246x |

The frustum speedup is far smaller than the radius speedup because the test
camera's far plane still covers a meaningful fraction of the world; a radius
query's search volume is fixed and small by construction, so it prunes far
more of the tree. Numbers are from one local run for illustration, not a
tracked regression benchmark.

## LOD (Phase 5)

`spatial::lod` selects a per-tile LOD index from camera distance, with
hysteresis to avoid flicker at a boundary. Full writeup in
[lod.md](lod.md); the short version: distance-threshold and screen-space-
error selection turn out to be the same algorithm (SSE mode converts its
error budget into equivalent distance thresholds), so there's one
hysteresis implementation, not two. `LODManager<Key>` is templated on an
opaque key for the same reason `SpatialIndex<T>` is generic — Core (and
`spatial::lod`, which only depends on Core) doesn't depend on the tile
model. LOD never triggers a tile load: the binary tile format bundles every
LOD into one file, so LOD is purely a render-time decision over
already-resident data, independent of and testable apart from streaming.

## Streaming (Phase 6 — core milestone)

`spatial::streaming::StreamingManager` is the orchestrator described in the
project brief: given a camera, it determines desired tiles
(`TileIndex::queryRadius`), diffs against what's tracked, issues/cancels
requests, and drives each tile through the resource state machine
(`ResourceState.h`) — asserted valid on every transition, never silently
skipped. `RequestQueue` is a thread-safe priority queue (lazy-deletion
cancellation); `WorkerPool` owns a fixed thread pool that pulls from it and
runs an injected `TileLoader`, reporting results back through a
mutex-guarded queue `update()` drains on the main thread. Full design,
including the priority formula and how cancellation is scoped to "never
start wasted work, but let in-flight work finish and discard it," is in
[streaming.md](streaming.md).

Verified with a real end-to-end test
(`tests/streaming/StreamingIntegrationTests.cpp`): generates actual `.tile`
files via `SpatialTileBuilder`'s generator, builds a real `TileIndex` from
a manifest, and streams the real files from disk through `StreamingManager`
— not just against in-memory fakes.

Memory/GPU budgets and eviction (LRU or otherwise) are explicitly out of
scope here; a tile leaving the streaming radius unloads immediately. Phase
7 adds a cache with a retention policy on top of this baseline.

## Status

This document will be extended as each phase lands. Current state: Phase 6
(streaming manager, request queue, worker pool, state machine, cancellation,
priority) complete, on top of Phase 5 (LOD). No memory/GPU budget, cache
eviction, or rendering logic exists yet.
