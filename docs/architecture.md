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

## Status

This document will be extended as each phase lands. Current state: Phase 2
(core spatial types) complete. No tile/dataset model, spatial index,
streaming, LOD, culling policy, or rendering logic exists yet.
