# Changelog

All notable changes to this project are documented here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

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
  color-coded exactly per the project brief's legend (green/yellow/red/gray).
- `MockRenderer` (`tests/rendering/MockRenderer.h`) — a recording `IRenderer`
  test double used across the rendering and debug test suites.
- 23 new Catch2 test cases, including an end-to-end test that pulls a
  genuinely resident tile out of a real `StreamingManager` and uploads its
  meshes through `GPUUploadQueue` — proving Streaming and Rendering compose
  correctly despite neither depending on the other.
- Full design write-up, including why `StreamingManager` deliberately does
  *not* depend on `IRenderer` yet, in `docs/rendering.md` (new).

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
- Full design write-up (including why one formula replaces the brief's
  separate LRU/distance/priority/combined eviction options) in
  `docs/streaming.md`.

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
  LOD selection reuse the exact same distance-threshold algorithm as
  distance-mode selection (see `docs/lod.md` for why).
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
