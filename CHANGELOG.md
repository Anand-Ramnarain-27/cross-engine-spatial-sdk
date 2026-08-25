# Changelog

All notable changes to this project are documented here. Format loosely
follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

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
