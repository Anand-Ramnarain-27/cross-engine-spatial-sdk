# Changelog

All notable changes to this project are documented here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

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
