# Changelog

All notable changes to this project are documented here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

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
