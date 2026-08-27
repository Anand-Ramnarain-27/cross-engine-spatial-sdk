# Changelog

All notable changes to this project are documented here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Added — Phase 12: Custom Engine Integration

- `sdk/CMakeLists.txt` — `SPATIAL_SDK_CUSTOM_ENGINE_STAGE_DIR`, an
  opt-in cache variable (unset by default) that copies the built static
  library and public headers into an external project's own third-party
  layout after each build — the same role the Unity/Unreal native
  plugins' staging steps play, for a consuming project outside this
  repository.
- Validated against an independent custom engine — a DirectX 12 engine
  with a GameObject/Component architecture and a full ImGui editor, not
  built for this integration. The integration itself
  (`ComponentSpatialWorld`, `PhoenixSpatialRenderer`, and a handful of
  engine-file edits — the component type enum, the component factory,
  one line in the mesh-gather step, include/lib paths) lives in that
  engine's own repository; `docs/custom_engine_integration.md` documents
  the pattern and the result.
- **Direct C++ linkage, not a C ABI** — the contrast with both engine
  plugins, made possible because the consuming engine's MSVC toolset,
  per-config CRT linkage, and C++ standard match this repo's own CMake
  build exactly.
- **Zero coordinate conversion** — the target engine's math library is
  right-handed/Y-up with the same CCW-front-face convention the SDK's
  procedural mesh generator produces, matching the convention
  `spatial::core::Mat4` was given in Phase 2, so positions, normals, and
  winding cross the boundary unmodified. `PhoenixSpatialRenderer` is
  consequently the simplest of the three `IRenderer` implementations:
  `createMesh()`/`createMaterial()` construct the engine's own
  `Mesh`/`Material` objects directly, no CPU-buffer pull step.
- The engine's `Component::render()` virtual turned out to be dead code
  (empty in `ComponentMesh`, its only call site unreachable outside
  prefab editing) — the real render path is a per-frame gather step that
  `ComponentSpatialWorld::buildMeshEntries()` plugs into the same way the
  engine's own procedural-model path already does, rather than adding a
  parallel rendering path.
- Runtime-verified by running the built engine and reading a file-based
  log (the engine's built-in logging writes via `OutputDebugStringA`,
  which needs an attached debugger — a small file logger was added
  instead): streaming converges to `resident=16 loading=0 requested=0
  drawCommands=32`, the same numbers as the Unity and Unreal demos on the
  same dataset, and matched what was rendered on screen — solid,
  correctly shaded geometry within the debug tile-bounds overlay.

### Added — Phase 11: Unreal Integration

- `examples/UnrealDemo/NativePlugin` — a second, independent C++ CMake
  target (`SpatialUnrealPlugin.dll`) wrapping `spatial::SpatialWorld`
  behind its own flat C ABI (`SpatialUnrealPlugin.h/.cpp`) — not shared
  with `SpatialUnityPlugin.dll`: real cross-engine SDKs ship a plugin
  binary per engine, matching this project's own architecture diagram.
  Unreal plugins are C++-native, but Unreal pins its own MSVC
  toolset/CRT settings per engine release with no guarantee they match
  this repo's own CMake build, and a mismatch there is silent heap
  corruption at a C++ ABI boundary rather than a compile error — hence a
  C ABI here too. `examples/common/ManagedMeshRenderer` (shared with the
  Unity plugin, since it's genuinely engine-agnostic) is the `IRenderer`
  behind it, backing `UProceduralMeshComponent`.
- `UnrealCoordinateConversion.h` — right-handed/Y-up/meters (the SDK) to
  Unreal's left-handed/Z-up/centimeters: a bigger mismatch than Unity's
  (an axis swap plus 100x scale, not just a sign flip). This conversion
  lives natively in `SpatialUnrealPlugin.cpp` — every exported function
  hands back Unreal-space-ready data, so `USpatialWorldComponent` does no
  conversion math of its own, and the conversion is covered directly by
  this repo's Catch2 suite (`tests/examples/UnrealCoordinateConversionTests.cpp`,
  `SpatialUnrealPluginTests.cpp`).
- `UnrealProject/Plugins/SpatialSDKPlugin` — `USpatialWorldComponent`,
  linked against the native plugin as a prebuilt ThirdParty binary.
  Compiles clean via `UnrealBuildTool` against UE 5.6.1, `/W4`-clean on
  the native side. `SpatialSDKDemoGameMode`/`Pawn`/`Actor` spawn the demo
  entirely in code, so running it needs no hand-placed level content.
- Runtime-verified by running the compiled game and reading Unreal's own
  log: streaming converges to `resident=16 loading=0 requested=0
  drawCommands=32` (the full 4x4 tile grid) and holds steady across
  hundreds of frames with zero errors.
- Two bugs were found and fixed during testing: (1) the scene had no
  light source, so every `UProceduralMeshComponent` section rendered
  pure black against an equally black background —
  `SpatialSDKDemoGameMode` now spawns an `ADirectionalLight`; (2)
  `FMatrix` uses Unreal's row-vector convention (translation in the last
  row), the opposite of the SDK's column-vector `Mat4` (translation in
  the last column) — harmless while `SpatialWorld` only ever passes an
  identity transform, but a real bug waiting for the first non-identity
  one; `BuildTransform()` now transposes correctly. After both fixes:
  solid, correctly-lit buildings with visible light/shadow faces,
  confirming the coordinate-conversion winding fix visually as well as
  by unit test.
- 14 new automated SDK tests (6 exercising the `SpatialUnreal_*` C ABI
  end to end, 8 pure conversion-math tests), 181 total (up from 167),
  all green, including a full `SPATIAL_SDK_WARNINGS_AS_ERRORS=ON`
  rebuild.

### Added — Phase 10: Unity Integration

- `examples/UnityDemo/NativePlugin` — a C++ CMake target
  (`SpatialUnityPlugin.dll`) wrapping `spatial::SpatialWorld` behind a
  flat C ABI (`SpatialUnityPlugin.h/.cpp`), since Unity's P/Invoke
  marshaler can only cross into C++ through free functions and
  fixed-layout structs, not class members. `ManagedMeshRenderer` is the
  `IRenderer` implementation behind it: rather than a native Unity
  graphics-API plugin (`IUnityGraphicsD3D11`/`GL.IssuePluginEvent`, a
  separate, pipeline-specific project), it records CPU-side
  mesh/material/draw-command data that Unity pulls once per frame and
  turns into real `UnityEngine.Mesh` objects drawn with
  `Graphics.DrawMesh` — see `docs/unity_integration.md`.
- `UnityProject/Assets/SpatialSDK/Scripts/` — `SpatialWorldNative.cs`
  (`[DllImport]` bindings kept in sync with the C header by hand),
  `CoordinateConversion.cs` (right-handed SDK to left-handed Unity: Z
  negation plus reversed triangle winding), and `SpatialWorldComponent.cs`
  — the `MonoBehaviour` an integration adds to a `GameObject`, exposing
  dataset path, streaming config, debug/statistics toggles, and material
  as Inspector fields, with no core SDK logic of its own.
- A real, openable Unity project (`UnityProject/`, Unity 6000.5.9f1) with
  a generated demo dataset and `Assets/Scenes/SpatialSDKDemo.unity`
  (reproducible via the `Spatial SDK > Rebuild Demo Scene` Editor menu
  command). Verified live in the Unity Editor: entering Play mode
  streams the dataset to 16/16 tiles resident, with the rendered
  buildings solid and correctly shaded — confirming the
  coordinate-conversion winding fix — and zero console errors or
  warnings.
- `tests/examples/SpatialUnityPluginTests.cpp` — 6 new test cases
  exercising the exported `SpatialUnity_*` C functions directly (not
  just the C++ classes behind them), covering struct layout,
  caller-allocated output buffers, and unknown-id handling. 167
  automated SDK tests total (up from 161).

### Added — post-Phase-9 polish: `SpatialWorld` API façade and fixes

- `spatial::SpatialWorld` (`sdk/include/spatial/SpatialWorld.h`) — the
  public API façade every engine integration builds on:
  `loadDataset()`/`update()`/`render()`/`shutdown()`. Consolidates the
  `StreamingManager`+`LODManager`+`GPUUploadQueue`+`DebugRenderer`+
  per-tile-GPU-tracking wiring that used to live directly in
  `examples/StandaloneViewer/src/main.cpp` — that file now just calls
  `SpatialWorld`'s four methods. Documented in `docs/sdk_api.md` and
  `docs/architecture.md`.
- `DatasetManifest` gained real `worldHeightMin`/`worldHeightMax` fields
  (optional in the JSON manifest — an older manifest without them still
  gets the previous generic `[-1000, 1000]` range). `SpatialTileBuilder`
  now writes the actual height range it generates, fixing at the root
  the bug the Phase 9 debug-wireframe fix had only worked around:
  `TileIndex`'s bounds are now accurate by construction.
- `FlyCamera` (previously untested) now has 9 unit tests: direction
  vectors, mouse-look sign convention and pitch clamping, movement along
  each local axis, view-matrix correctness, and `CameraParams`
  conversion.
- Two use-after-free bugs were found and fixed while building this: (1)
  GPU upload callbacks capturing a reference to a tile's GPU record that
  could be evicted before the upload completes, fixed with deferred
  erasure; (2) `SpatialWorld` declared before its `IRenderer` in both
  `main.cpp` and, independently, in the test suite, meaning the renderer
  could be destroyed before the GPU resources pointing into it — both
  documented inline everywhere the pattern recurs.
- A real screenshot in the README, from the viewer running against a
  1,024-tile generated dataset.
- 161 automated SDK tests total (up from 143), all green, stable across
  repeated runs, including with `SPATIAL_SDK_WARNINGS_AS_ERRORS=ON`.

### Added — Phase 9: Standalone Viewer (first complete demonstration)

- `examples/StandaloneViewer` — the SDK's first end-to-end application.
  Win32 window + input (`Win32Window`), the first real `IRenderer`
  implementation (`D3D11Renderer`, Direct3D 11, shaders compiled at
  startup from `assets/shaders/*.hlsl`), a free-fly camera (`FlyCamera`),
  and a `main.cpp` that wires `StreamingManager`, `LODManager<TileId>`,
  `GPUUploadQueue`, and `DebugRenderer` together against a real dataset.
- `assets/shaders/Mesh.hlsl` and `DebugLine.hlsl` — both declared
  `row_major` and using `mul(matrix, vector)` to match `Mat4`'s
  row-major/column-vector convention exactly (getting this wrong
  silently transposes every transform rather than erroring — called out
  in both the shader source and `docs/rendering.md`).
- CLI: `--dataset`, `--tiles`, `--assets`, `--width`/`--height`,
  `--streaming-radius`, `--max-resident-tiles`, `--cpu-budget-mb`,
  `--worker-threads`, `--run-seconds` (auto-exit, used for automated
  smoke testing). Controls: WASD move, Space/Ctrl up/down, hold right
  mouse button to look, F1 toggles the debug tile-bounds overlay, Esc quits.
- Fixed: GPU upload callbacks capture a reference to their tile's
  per-frame GPU record, but a tile can be evicted (erasing that record)
  before its upload finishes — `main.cpp` now defers erasure until every
  in-flight upload for a tile has completed.
- Fixed: the debug tile-bounds wireframe used `TileIndex`'s bounds,
  whose Y range comes from `DatasetManifest::worldBounds()`'s generic
  `[-1000, 1000]` placeholder (no real per-dataset height field existed
  yet — see `docs/tile_format.md`), making every wireframe box 2000
  units tall instead of hugging its tile. `main.cpp` now prefers a
  resident tile's own tightly-bounded `bounds()` for the debug draw,
  falling back to `TileIndex` only for tiles not yet loaded.
- Verified end-to-end via automated `--run-seconds` smoke testing (real
  dataset generated by `SpatialTileBuilder`, real window, real D3D11
  device, zero load failures, clean exit), CLI error-path testing
  (missing `--dataset`, nonexistent dataset path, `--help`), and visual
  verification of the rendered city and color-coded debug wireframe.
  Full build, including the viewer, passes with
  `SPATIAL_SDK_WARNINGS_AS_ERRORS=ON` and zero warnings.

### Added — Phase 8: Rendering

- `spatial::rendering::IRenderer` — the engine-agnostic GPU abstraction
  (create/destroy mesh/material/texture, submit draws, submit batched debug
  lines). No concrete backend yet; that's Phase 9.
- `Handle<Tag>` (`ResourceHandle.h`) — phantom-typed opaque handles
  (`MeshHandle`, `TextureHandle`, `MaterialHandle`).
- `GPUResource<HandleT, Destroy>` (`GPUResource.h`) — one RAII
  implementation, via a non-type template parameter for the destroy member
  function, backing all three `*Resource` aliases instead of three
  hand-written copies.
- `GPUUploadQueue` — bounds GPU uploads processed per call; this is what
  makes `ResourceState::UploadPending` a real step Phase 9 can make
  asynchronous, rather than the instant StreamingManager-internal
  pass-through it's been since Phase 6.
- `spatial::debug::DebugRenderer` and `colorForState()` — batches tile
  bounding-box wireframes into one `drawDebugLines` call per flush,
  color-coded per the project's state legend (green/yellow/red/gray).
- `MockRenderer` (`tests/rendering/MockRenderer.h`) — a recording `IRenderer`
  test double used across the rendering and debug test suites.
- 23 new Catch2 test cases, including an end-to-end test that pulls a
  genuinely resident tile out of a real `StreamingManager` and uploads its
  meshes through `GPUUploadQueue` — proving Streaming and Rendering compose
  correctly despite neither depending on the other.
- Full design write-up, including why `StreamingManager` deliberately does
  not depend on `IRenderer` yet, in `docs/rendering.md` (new).

### Added — Phase 7: Cache and Memory Budget

- `spatial::data::estimateTileMemoryBytes` (`TileMemory.h`) — deliberately
  inexact CPU memory estimate for a `Tile` (vertex/index buffers dominate),
  used to enforce a budget rather than for precise accounting.
- `spatial::streaming::TileCache` — owns CPU data for every resident tile;
  evicts by a combined priority + recency score
  (`keepScore = lastPriority - recencyWeight * framesSinceLastTouched`)
  against a CPU-byte, GPU-byte (abstraction), and tile-count budget.
  Tiles desired the current frame are never evicted.
- `StreamingManager` now retains a tile that falls out of the streaming
  radius (previously: immediate unload) instead of unloading it — it stays
  Resident, cached, until `TileCache` actually evicts it under budget
  pressure. `StreamingStatistics` gained `totalCacheHits`,
  `cpuMemoryUsedBytes`, and `gpuMemoryUsedBytes`.
- 18 new Catch2 test cases: `TileMemory` estimate scaling, `TileCache`
  put/touch/find, protected-id and tile-count/byte-budget eviction,
  combined-score ordering; plus `StreamingManager` tests for cache-hit
  reuse (no reload when a tile re-enters the streaming radius) and
  budget-driven eviction under a sweeping-camera scenario. One pre-existing
  Phase 6 test (immediate-unload-on-leaving-radius) was rewritten — that
  behavior is intentionally gone as of this phase.
- Full design write-up (including why one formula replaces separate
  LRU/distance/priority/combined eviction options) in `docs/streaming.md`.

### Added — Phase 6: Streaming (core milestone)

- `spatial::streaming::ResourceState` — the full tile resource lifecycle
  (`Unloaded` through `Resident` and back) with `isValidTransition`,
  asserted on every transition by `StreamingManager`.
- `RequestQueue` — thread-safe priority queue with lazy-deletion
  cancellation and push-time deduplication.
- `WorkerPool` — a fixed-size thread pool pulling from a `RequestQueue`,
  running an injected `TileLoader`, and reporting results/in-flight status
  back through mutex-guarded state the main thread polls each frame.
- `StreamingManager` — determines desired tiles from `TileIndex::queryRadius`,
  issues distance/direction-weighted prioritized requests (throttled per
  `update()` call), cancels requests that haven't started, discards results
  for in-flight loads that are no longer wanted, and unloads resident tiles
  that fall outside the streaming radius. `makeFileTileLoader()` builds the
  standard disk-backed loader.
- 34 new Catch2 test cases: exhaustive state-machine transition coverage,
  `RequestQueue` concurrency/dedup/cancellation (including a 4-producer/
  3-consumer stress test), `WorkerPool` dispatch and in-flight tracking
  (synchronized via `std::promise`, not sleeps), and `StreamingManager`
  integration tests covering priority ordering, both cancellation paths,
  failure handling, and request throttling — driven by a
  `ControllableLoader` test double for deterministic async timing.
- A real end-to-end test (`StreamingIntegrationTests.cpp`) that generates
  actual `.tile` files, builds a real `TileIndex`, and streams them from
  disk — not just in-memory fakes.
- Full design write-up in `docs/streaming.md`.

### Added — Phase 5: LOD

- `spatial::lod::selectLODByDistance` and `computeScreenSpaceError`/
  `screenSpaceErrorCrossoverDistance` — the latter lets screen-space-error
  LOD selection reuse the same distance-threshold algorithm as
  distance-mode selection (see `docs/lod.md`).
- `LODManager<Key>` — stateful, hysteresis-aware LOD selection keyed by an
  opaque hashable key (`TileId` in practice), templated so `spatial::lod`
  stays dependency-free of the tile model, matching the pattern already
  used for `SpatialIndex<T>`.
- `core::CameraParams` — the minimal per-frame camera snapshot shared by
  LOD selection and (Phase 6) streaming.
- 16 new Catch2 test cases covering both LOD metrics and hysteresis
  behavior (holding steady near a boundary, releasing past it, snapping on
  a large jump, per-key independence).

### Added — Phase 4: Spatial Hierarchy

- `spatial::core::SpatialIndex<T>` (`sdk/include/spatial/core/SpatialIndex.h`)
  — a generic quadtree over axis-aligned bounds (X/Z split), with
  `queryFrustum`, `queryRadius`, and `queryAABB`. Zero dependency on the
  tile/dataset model, per the Core layering rule.
- `spatial::data::TileIndex` — wraps `SpatialIndex<TileId>` plus an
  `unordered_map` for O(1) id lookup. `TileIndex::buildUniformGrid` computes
  every tile's bounds from the dataset manifest alone (no `.tile` files
  read).
- 20 new Catch2 test cases: `SpatialIndex` correctness against brute-force
  comparison on random data (frustum + radius queries), subdivision and
  boundary-straddling edge cases, and `TileIndex` grid construction/lookup/
  validation.
- `tests/core/SpatialIndexBenchmark.cpp` — Catch2 `BENCHMARK` comparing
  brute-force vs. indexed frustum/radius queries over 10,000 items; excluded
  from the default `ctest` run (hidden tag), run manually. Results recorded
  in `docs/architecture.md`.
- Corrected a `docs/tile_format.md` inaccuracy from Phase 3: `tileSize`
  describes each generated tile's footprint, not a "level-0 root tile" —
  `SpatialTileBuilder` generates one flat grid of tiles at a single
  `TileId.level`, and per-tile LOD is a separate axis from quadtree depth.

### Added — Phase 3: Tile Format

- `spatial::Expected<T>`/`Error`/`ErrorCode` (`sdk/include/spatial/Error.h`)
  — the SDK's exception-free error-reporting mechanism, per `docs/architecture.md`.
- `spatial::data` model: `TileId`, `Metadata`, `Material`, `Mesh`/`Vertex`,
  `TileLOD` (one `Mesh` per material), `Tile`, `DatasetManifest`.
- Binary `.tile` format read/write (`TileSerializer`) and JSON dataset
  manifest read/write (`DatasetSerializer`, using a privately-linked
  `nlohmann::json`); full schema documented in `docs/tile_format.md`.
- `SpatialTileBuilder` CLI tool (`tools/TileBuilder/`): generates a
  deterministic procedural city (ground + buildings, multiple LOD levels)
  and writes a complete dataset (manifest + tiles) in the SDK's format.
- 28 new Catch2 test cases covering `Expected`/`Error`, `TileId`, tile
  serialization round-trips and corruption handling (bad magic, truncation,
  unsupported version), dataset manifest round-trips and validation, and the
  procedural generator's determinism/LOD-thinning behavior.

### Added — Phase 2: Core Spatial Types

- `spatial::core` vector types: `Vec2`, `Vec3`, `Vec4` (header-only,
  `sdk/include/spatial/core/`).
- `Mat4` (row-major storage, column-vector convention) with translation,
  scale, perspective, and look-at construction; documented in
  `docs/architecture.md`.
- `Plane`, `AABB`, `Sphere`, `Frustum` bounding volumes, including
  `Frustum::fromViewProjection` (Gribb/Hartmann plane extraction) and
  `intersectsAABB`/`intersectsSphere` culling tests.
- `CoordinateSystem` enum with string conversion, matching the dataset
  manifest's `coordinateSystem` field.
- 27 new Catch2 test cases (`tests/core/`) covering arithmetic, matrix
  composition/lookAt/perspective, bounding-volume intersection edge cases,
  and frustum classification.

### Added — Phase 1: Project Foundation

- CMake project (`CMakeLists.txt`) with options for shared/static build,
  tests, tools, examples, and warnings-as-errors.
- `spatial_sdk` core library target with a minimal public header
  (`spatial/Version.h`, `spatial/Export.h`).
- Catch2-based test suite wired into CTest (`tests/`).
- Repository skeleton matching the target layout: `docs/`, `tools/`,
  `examples/{StandaloneViewer,UnityDemo,UnrealDemo,CustomEngineDemo}/`,
  `assets/{datasets,shaders}/`, `scripts/`.
- Initial documentation: `README.md`, `docs/architecture.md`,
  `docs/getting_started.md`, and stubs for the remaining docs pages.
- GitHub Actions CI building and testing on Windows (MSVC).
- `LICENSE` (MIT).
