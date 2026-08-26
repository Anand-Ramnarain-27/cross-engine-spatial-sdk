# Getting Started

## Requirements

- Windows (initial supported platform; platform-specific code is isolated so
  other platforms can be added later)
- CMake 3.21+
- A C++20 compiler (MSVC from Visual Studio 2022 is what this project is
  developed against; Visual Studio's bundled CMake/Ninja under
  `Common7\IDE\CommonExtensions\Microsoft\CMake` work fine if CMake isn't on
  your `PATH`)
- Internet access on first configure (CMake `FetchContent` pulls Catch2 and
  nlohmann/json)

## Build

From the repository root:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Or, with Ninja (faster incremental builds; requires a Developer Command
Prompt / `vcvarsall.bat`-initialized shell so `cl.exe` is on `PATH`):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run the tests

```bash
ctest --test-dir build -C Debug --output-on-failure
```

## Build options

| Option | Default | Meaning |
|---|---|---|
| `SPATIAL_SDK_BUILD_SHARED` | `OFF` | Build `spatial_sdk` as a DLL instead of a static library |
| `SPATIAL_SDK_BUILD_TESTS` | `ON` | Build the Catch2 test suite |
| `SPATIAL_SDK_BUILD_TOOLS` | `ON` | Build command-line tools (`SpatialTileBuilder`) |
| `SPATIAL_SDK_BUILD_EXAMPLES` | `ON` | Build example/demo applications |
| `SPATIAL_SDK_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |

## Generate a sample dataset

`SpatialTileBuilder` writes a procedural city dataset (manifest + tiles) in
the SDK's format — see [tile_format.md](tile_format.md) for the schema:

```bash
build/bin/Debug/SpatialTileBuilder.exe --output assets/datasets/demo --name DemoCity --grid 4 --tile-size 100 --max-lod 3
```

`--help` lists every option (grid size, building count/height range, RNG
seed for reproducible layout, etc.).

## Run the standalone viewer

`StandaloneViewer` needs a dataset generated above, and must be able to
find `assets/shaders/` (its HLSL is compiled at runtime) — run it from the
repository root, or pass `--assets <dir>`:

```bash
build/bin/Debug/StandaloneViewer.exe --dataset assets/datasets/demo/DemoCity.world
```

Controls: `WASD` to move, `Space`/`Ctrl` for up/down, hold the right mouse
button to look around, `F1` toggles the debug tile-bounds overlay, `Esc`
quits. `--help` lists every option, including `--run-seconds N` (auto-exit
after N seconds — used for automated smoke testing, not needed for normal
use). See [rendering.md](rendering.md) for how the viewer's Direct3D 11
backend fits into the SDK's rendering abstraction.

## Project layout

See [architecture.md](architecture.md) for the module layering. In short:

- `sdk/include/spatial/` — public headers (the only headers engine
  integrations should include)
- `sdk/src/` — implementation, private headers
- `tests/` — Catch2 unit tests, mirroring the `sdk/` module structure
- `tools/` — standalone command-line tools
- `examples/` — demo applications, one per target (standalone, Unity, Unreal,
  custom engine)
