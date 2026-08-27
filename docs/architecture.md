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

## Cache and memory budget (Phase 7)

`spatial::streaming::TileCache` sits between the state machine and the
actual `data::Tile` CPU data: once a load completes, `StreamingManager`
hands the tile to the cache rather than holding it itself, and a tile
leaving the streaming radius stays Resident (cached) instead of unloading
immediately. Eviction is budget-driven — CPU bytes, a GPU-bytes
abstraction (mirrors the CPU estimate; there's no real GPU resource to
measure until Phase 8), and a tile-count cap — using a combined
priority-plus-recency score rather than pure LRU or pure distance. Tiles
desired this frame are never evicted, even over budget. Full design,
including the exact eviction formula and why it subsumes the project
brief's separate "LRU / distance / priority / combined" strategy options
into one tunable weight, is in [streaming.md](streaming.md).

## Rendering (Phase 8)

`spatial::rendering::IRenderer` is the engine-agnostic GPU boundary
(create/destroy meshes, materials, textures; submit draws; submit batched
debug lines) — minimal on purpose, no render passes or pipeline state.
`GPUResource<HandleT, Destroy>` gives every resource type
(`MeshResource`/`MaterialResource`/`TextureResource`) the same RAII
lifetime handling from one template instead of three copies of it.
`GPUUploadQueue` bounds how many uploads happen per call, which is what
`ResourceState::UploadPending` needs to become a real asynchronous step
instead of the instant pass-through it's been since Phase 6.
`spatial::debug::DebugRenderer` batches tile-bounds wireframes
color-coded by `ResourceState`, matching the project brief's legend
exactly. Full design in [rendering.md](rendering.md).

At the time this was written no real backend existed — everything was
verified against a recording `MockRenderer` test double
(`tests/rendering/MockRenderer.h`). Phase 9 (below) adds the first real one.
`StreamingManager` still has no dependency on `IRenderer` — Streaming and
Rendering remain independent branches under the API layer; Phase 9's
`examples/StandaloneViewer/src/main.cpp` is the orchestration code that
wires the two together, exactly as anticipated here.

## `SpatialWorld` — the public API façade

`spatial::SpatialWorld` (`sdk/include/spatial/SpatialWorld.h`) is the
single object an engine integration owns, instead of separately wiring up
`StreamingManager`, `LODManager`, `GPUUploadQueue`, and `DebugRenderer`
itself — which is exactly what `examples/StandaloneViewer/src/main.cpp`
did in Phase 9's first version, before this class existed. `main.cpp` now
calls `loadDataset()`/`update()`/`render()`/`shutdown()` and nothing else;
every subsystem it used to wire together by hand now lives inside
`SpatialWorld`. This is the payoff for keeping those subsystems
independent all along (Streaming not depending on Rendering, etc.) — one
façade can compose them without any of them depending on each other.

It owns the per-resident-tile GPU bookkeeping directly: every LOD's meshes
uploaded up front (a tile file already contains them all — see
[lod.md](lod.md)), `StreamingManager` says which tiles are resident,
`LODManager` says which already-uploaded LOD to draw each frame,
`GPUUploadQueue` bounds how many uploads happen per `update()` call, and
`DebugRenderer` draws every in-radius tile's bounds color-coded by
`StreamingManager::stateOf()`.

Deliberately does *not* introduce `SpatialDataset`/`Camera` wrapper classes
just to match the project brief's suggested class list — `DatasetManifest`
and `CameraParams` already exist and carry no missing behavior that a
wrapper would add; per the project's "don't over-engineer" rule, an empty
wrapper isn't worth the extra indirection.

**Two real bugs surfaced while building and testing this — both are the
same lesson, learned twice:**

1. *Use-after-free, upload callbacks vs. eviction.* Upload callbacks
   capture a reference to their tile's GPU record, but a tile can be
   evicted from `StreamingManager` before an upload queued for it finishes
   (a real race once tiles move faster than the upload budget can drain).
   `SpatialWorld` defers actually erasing that record until every upload
   that captured a reference to it has completed, rather than erasing as
   soon as the tile stops being desired.
2. *Use-after-free, declaration order.* `SpatialWorld`'s GPU resources hold
   pointers into whatever `IRenderer` created them. In `main.cpp`, `world`
   is declared after `renderer` specifically so it's destroyed *first* (C++
   destroys locals in reverse declaration order) on every exit path,
   including an exception unwinding the scope — getting this backwards
   compiles fine and only crashes at teardown. This bug was reintroduced
   independently in `tests/SpatialWorldTests.cpp` (declaring `world` before
   `renderer` there too) and caught the same way: a SIGSEGV at scope exit
   that only appeared once the tests actually exercised full upload
   completion, not at compile time. Every test in that file now documents
   the ordering requirement inline rather than relying on it being obvious.

**The manifest-height-bounds bug from the first Phase 9 pass is now fixed
at the root**, not just worked around: `DatasetManifest` gained real
`worldHeightMin`/`worldHeightMax` fields (optional in the JSON — an older
manifest without them still gets the old generic `[-1000, 1000]` range, so
nothing existing breaks), and `SpatialTileBuilder` now writes the actual
height range it generates. `TileIndex`'s bounds are accurate by
construction now; `SpatialWorld`'s debug draw still prefers a resident
tile's own (tightest) bounds when available, as defense in depth. See
[tile_format.md](tile_format.md).

Verified: the full pipeline (dataset generation, real window, real D3D11
device, streaming, upload, draw) runs cleanly end-to-end via
`--run-seconds` automated smoke testing and on-screen visual confirmation.
After the `SpatialWorld` refactor, a fresh screenshot (in the README)
confirmed identical behavior to before the refactor — same final stats
(54 resident tiles, 0 load failures) on the same test dataset.

## Unity integration (Phase 10)

`examples/UnityDemo` is the SDK's first engine integration, and the first
real test of the boundary `IRenderer` was designed around: can something
that isn't a C++ application built by this repo's own CMake consume the
SDK at all? The answer required a native plugin, since C# cannot call C++
member functions or see a C++ class's layout — everything crossing that
boundary is a flat `extern "C"` function and fixed-layout structs
(`SpatialUnityPlugin.h`, mirrored by hand in `SpatialWorldNative.cs`, kept
in sync without codegen).

The interesting design decision was how far to take the rendering side.
`IRenderer` doesn't care what a backend does with the geometry it's
handed — `D3D11Renderer` uploads it to a real GPU device; a native Unity
integration would normally do the same against Unity's own device via its
low-level rendering plugin interface (`IUnityGraphicsD3D11`,
`GL.IssuePluginEvent`), which is real, valid, and also a meaningfully
separate project — the interop differs by graphics API and by
Built-in/URP/HDRP, and mistakes there tend to fail on driver-specific
timing rather than at compile time. `ManagedMeshRenderer` — the
`IRenderer` implementation the plugin actually ships — takes the other
option: it never touches a graphics API. `createMesh`/`drawMesh`/
`drawDebugLines` just record CPU-side data; Unity pulls it once per frame
across the C API and turns it into real `UnityEngine.Mesh` objects drawn
with `Graphics.DrawMesh`. This is a legitimate, commonly used pattern for
streaming plugins (not unique to this project), works unmodified under
any Unity render pipeline since it never assumes one, and — same as the
D3D11-vs-D3D12/Vulkan choice in Phase 9 — is flagged here rather than
presented as the only way to do it. Full writeup, including what this
trades away, in [unity_integration.md](unity_integration.md).

**A second engine-boundary convention mismatch, same shape as Phase 9's
HLSL one.** `spatial::core::Mat4` is right-handed, Y-up; Unity is
left-handed, Y-up. Same up axis, opposite handedness — negate Z on every
position/direction/normal crossing the boundary, and reverse each
triangle's winding to compensate (mirroring one axis flips front-face
winding). Both live in exactly one place (`CoordinateConversion.cs`).
Convention mismatches like this compile fine and only show up as visibly
wrong geometry, so this one was verified on screen in the Unity Editor:
entering Play mode confirmed the rendered buildings are solid and
correctly shaded, not mirrored or inside-out.

## Unreal integration (Phase 11)

A second, independent native plugin (`SpatialUnrealPlugin`,
`examples/UnrealDemo`) — not the same binary as `SpatialUnityPlugin`, and
not because it couldn't be shared: real cross-engine SDKs ship a plugin
per engine, and this repo's own architecture diagram already draws
Unity/Unreal/custom-C++ as three separate boxes hanging off the SDK, not
one binary all three share. The C ABI decision itself repeats for a
different reason than Unity's: Unity is forced into one by P/Invoke, but
Unreal plugins are C++-native, so linking `spatial_sdk.lib` directly into
the plugin module was the first option considered. It's rejected because
Unreal pins its own MSVC toolset/CRT settings per engine release with no
guarantee they match whatever this repo's own CMake build used — and a
mismatch there is undefined behavior at every `std::string`/`std::vector`
crossing the boundary, not a compile error. `ManagedMeshRenderer` (the
`IRenderer` behind both plugins) turned out to be genuinely
engine-agnostic once written, so it moved to `examples/common` and is now
shared — implementation reuse, not an architecture coupling between the
two plugins.

The coordinate conversion is a bigger mismatch than Unity's — Unreal is
left-handed *and* Z-up *and* centimeters, not just left-handed — and it's
handled differently on purpose: natively, inside `SpatialUnrealPlugin.cpp`,
rather than in host-language code the way Unity's `CoordinateConversion.cs`
does it. Every `SpatialUnreal_Get*` function hands back data that's
already Unreal-space; `USpatialWorldComponent` does no conversion math at
all. This is possible because the conversion needs to live in *some* C++
binary either way here (unlike Unity, there's no separate host-language
layer forcing the choice), and it means the single most error-prone part
of this boundary is provable by this repo's own Catch2 suite
(`UnrealCoordinateConversionTests.cpp`) instead of only by looking at the
screen — a real refinement over Phase 10's arrangement, not a retrofit of
it (Unity's plugin already shipped, verified, and nothing there needed
fixing).

`SpatialUnrealPlugin` compiled `/W4`-clean through this repo's own CMake
and is covered by 14 new Catch2 tests; `USpatialWorldComponent` and the
demo project compiled clean through real `UnrealBuildTool` against
installed UE 5.6.1; running the compiled game showed streaming actually
converge (`resident=16 loading=0 requested=0 drawCommands=32`, the full
dataset, steady across hundreds of frames, zero log errors).

Two bugs turned up during on-screen testing: (1) the scene had no light
source at all, and Unreal's default material is lit, not unlit, so
everything rendered as a black viewport — `SpatialSDKDemoGameMode` now
spawns an `ADirectionalLight`; (2) while fixing that, a second, unrelated
bug was caught by inspection: `FMatrix` uses Unreal's row-vector
convention (translation in the last row) — the opposite of the SDK's
column-vector `Mat4` (translation in the last column), confirmed against
the engine's own `TranslationMatrix.h` — so `BuildTransform()` was
silently transposing (invisible until a non-identity transform is ever
passed, since identity is its own transpose). After both fixes, the
rendered buildings are solid and correctly lit with real light/shadow
faces — visual confirmation, not just a unit test, that the
coordinate-conversion winding fix is correct. See
`examples/UnrealDemo/README.md` for the full verification writeup.

## Custom engine integration (Phase 12)

Validated against an independent custom engine — a DirectX 12
GameObject/Component engine with a full ImGui editor, not built for this
integration. The integration itself lives in that engine's own
repository (`ComponentSpatialWorld`, `PhoenixSpatialRenderer`, and a
handful of engine-file edits); `docs/custom_engine_integration.md`
documents the pattern and the result.

Two decisions define this phase:

1. **Direct C++ linkage, not a third C ABI.** Unlike Unity (forced into
   one by P/Invoke) and Unreal (chosen because of a real, checked
   toolset-mismatch risk — see the Unreal section above), a custom
   engine built with the *same* toolchain as this repo's own CMake build
   doesn't need one. This was confirmed by comparing the target engine's
   `.vcxproj` settings (v143/C++20, `/MD` Release / `/MDd` Debug)
   directly against this repo's own CMake output — a match. Had it not
   matched, a C ABI would have been the right call here too, for the
   same reason it was for Unreal.
2. **Zero coordinate conversion.** The target engine's math library
   (DirectXTK SimpleMath) is right-handed/Y-up with the same
   CCW-front-face convention `spatial::core::Mat4` and the SDK's
   procedural mesh generator already use. This is the payoff of choosing
   that convention for `Mat4` in Phase 2 rather than defaulting to
   whatever DirectX examples typically use (left-handed) —
   `PhoenixSpatialRenderer` is consequently the simplest of the three
   `IRenderer` implementations, constructing the engine's own
   `Mesh`/`Material` objects directly with no CPU-buffer intermediate
   step.

Tracing the engine's actual render path (rather than trusting
`Component::render()`'s signature at face value) turned up a real
architectural finding: that virtual is dead code in this engine (empty
in `ComponentMesh`, unreachable outside prefab editing in the only call
site). The live path is a per-frame gather step
(`EditorRenderer.cpp`'s `collectMeshes`) that walks the scene reading
each component's exposed geometry — `ComponentSpatialWorld` plugs into
that exact pattern (`buildMeshEntries()`, mirroring
`Model::buildMeshEntries()`) rather than adding a parallel rendering
path the engine wouldn't actually invoke.

Verified by running the built engine and reading a file-based log (the
engine's own logging writes via `OutputDebugStringA`, which needs an
attached debugger — this integration adds its own file logger instead):
streaming converges to `resident=16 loading=0 requested=0
drawCommands=32` — the same numbers, on the same generated dataset, as
the Unity and Unreal demos, held steady for the full session with zero
errors — and matched the rendered result on screen: solid, correctly
shaded geometry inside the debug tile-bounds overlay, confirming the
zero-conversion assumption held.

## Profiling (Phase 13)

`spatial::debug::Profiler` — always-on CPU frame timing built into
`SpatialWorld`, not a bolt-on external tool. Placed in the Debug module
alongside `DebugRenderer`, matching the module map above's original
"statistics, profiling instrumentation" line. A fixed `ProfileSection`
enum (`StreamingUpdate`, `GPUUpload`, `LODSelection`, `DebugDraw`) is
measured via RAII scope guards (`ScopedSection`) inside `update()` and
`render()`, on every exit path, and read back through
`SpatialWorld::frameProfile()`.

`StandaloneViewer --profile-csv <path>` exports one row per frame; run
against the 1024-tile `BigCity` dataset it shows the expected shape —
an initial-load spike, an upload-dominated ramp while streaming
converges, then a steady ~1–2ms/frame dominated by LOD selection once
`loadingTiles`/`requestedTiles` reach zero. See
[profiling.md](profiling.md) for the full design and reading the output.

## Portfolio polish (Phase 14, in progress)

All 13 engineering phases are complete; Phase 14 adds no new SDK
functionality by design — it consolidates design rationale and measured
results into [case_study.md](case_study.md) (why each design decision was
made, every real bug found during integration with root cause and fix,
and a streaming-vs-full-residency stress test across three dataset sizes
up to 16,384 tiles) and tightens the top-level documentation for a reader
seeing the project for the first time.

## Status

This document will be extended as each phase lands. Current state: Phase
14 (portfolio polish: case study write-up, stress test, documentation
pass) in progress, on top of Phase 13 (CPU frame profiler built into
`SpatialWorld`, verified against a real 1024-tile dataset run), Phase 12
(custom-engine integration, direct C++ linkage, validated against a real
DirectX 12 engine), Phase 11 (Unreal native plugin +
`USpatialWorldComponent`, compiled and run against real UE 5.6.1), Phase
10 (Unity native plugin + C# integration, verified live in the Unity
Editor), Phase 9 (standalone viewer, D3D11 backend, `SpatialWorld` public
API façade, full Streaming+LOD+Rendering+Debug wiring, real per-dataset
height bounds), and Phases 4–8. 187 automated SDK tests (the viewer and
all three engine integrations are GPU/Editor-dependent and aren't part of
that suite — see [rendering.md](rendering.md) for why platform backends
are verified by actually running them instead).
