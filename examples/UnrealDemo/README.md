# UnrealDemo

Unreal Engine plugin integration (Phase 11). Full design and rationale in
[docs/unreal_integration.md](../../docs/unreal_integration.md).

```
NativePlugin/          C++ CMake target -> SpatialUnrealPlugin.dll (the C ABI + native coordinate conversion)
UnrealProject/           a real, openable Unreal project (UE 5.6)
```

## Try it

1. Build the plugin (part of the normal top-level build; requires Windows):

   ```bash
   cmake --build build --config Debug --target SpatialUnrealPlugin
   ```

   This stages `SpatialUnrealPlugin.dll`/`.lib`/header into
   `UnrealProject/Plugins/SpatialSDKPlugin/Source/ThirdParty/SpatialUnrealPlugin/`
   automatically.

2. Generate the demo dataset (not committed — regenerate it):

   ```bash
   build/bin/Debug/SpatialTileBuilder --output examples/UnrealDemo/UnrealProject/SpatialSDKData/DemoCity --name DemoCity --grid 4 --tile-size 50 --max-lod 2 --buildings-per-tile 3 --seed 42
   ```

3. Open `UnrealProject/SpatialSDKDemo.uproject` in Unreal Engine (tested
   against 5.6.1) and press Play. `SpatialSDKDemoGameMode` spawns the
   camera and the demo actor in code — see
   `Source/SpatialSDKDemo/SpatialSDKDemoGameMode.h` — so there's nothing to
   place in the level by hand; `Content/Maps/DemoMap.umap` is deliberately
   empty and reproducible via `Content/Python/build_demo_map.py`.

## What was actually verified, and how

Compiled through the full real toolchain, twice over: `SpatialUnrealPlugin`
via this repo's CMake (with `SPATIAL_SDK_WARNINGS_AS_ERRORS=ON`, plus
`tests/examples/SpatialUnrealPluginTests.cpp` and
`UnrealCoordinateConversionTests.cpp`, 14 Catch2 test cases exercising the
exported C ABI and the coordinate-conversion math directly), and
`SpatialSDKPlugin`/`SpatialSDKDemo` via `UnrealBuildTool` against real
installed UE 5.6.1 — both succeeded cleanly.

Runtime behavior was verified by actually running the compiled game
(`UnrealEditor.exe SpatialSDKDemo.uproject -game`) and reading Unreal's own
log (`Saved/Logs/SpatialSDKDemo.log`, not a fabricated summary): the
dataset loaded successfully, and streaming converged to a stable
`resident=16 loading=0 requested=0 drawCommands=32` — the full 4x4 tile
grid, matching the Unity demo's equivalent numbers on the same
dataset — held steady across several hundred frames with zero errors or
warnings in the log.

**Visual confirmation, and two real bugs it caught.** A computer-use
limitation (the same one hit with `StandaloneViewer` in Phase 9 — a
custom-launched window outside the Start-Menu-registered app list isn't
screenshot-able in this environment) meant the session that built this
couldn't see the rendered output itself — so the user opened the project
in their own Editor and pressed Play. First result: a black viewport.
Root cause — the scene had no light at all, and Unreal's default material
is lit, not unlit, so every `UProceduralMeshComponent` section rendered
pure black against an also-black empty-sky background.
`SpatialSDKDemoGameMode` now spawns an `ADirectionalLight` alongside the
demo actor. While fixing that, a second, unrelated bug was caught by
inspection rather than by symptom: `FMatrix` uses Unreal's row-vector
convention (translation in the last *row*), the opposite of the SDK's
column-vector `Mat4` (translation in the last *column*) — confirmed
against the engine's own `TranslationMatrix.h`, not assumed. Harmless
today since `SpatialWorld` only ever passes an identity transform
(identity is its own transpose), but a real bug waiting for the first
non-identity one; `BuildTransform()` in `SpatialWorldComponent.cpp` now
transposes correctly. After both fixes: solid, correctly-lit buildings —
visible light and shadow faces, no mirroring, no inside-out geometry —
confirming the coordinate-conversion winding fix (already unit tested,
now also seen) is correct.

## Layout

```
NativePlugin/src/
  SpatialUnrealPlugin.h/.cpp        flat C ABI wrapping spatial::SpatialWorld, with native coordinate conversion
  UnrealCoordinateConversion.h      the one place right-handed/meters <-> Unreal's left-handed/Z-up/cm is handled

UnrealProject/
  SpatialSDKDemo.uproject
  Source/SpatialSDKDemo/            minimal primary game module + GameMode/Pawn/Actor that auto-spawn the demo
  Plugins/SpatialSDKPlugin/
    Source/SpatialSDKPlugin/        USpatialWorldComponent — the one component an integration adds
    Source/ThirdParty/SpatialUnrealPlugin/   staged DLL/.lib/header (gitignored, build artifact)
  Content/Maps/DemoMap.umap         deliberately empty; Content/Python/build_demo_map.py regenerates it
  SpatialSDKData/DemoCity/          demo dataset (gitignored, regenerate — see above)
```

`examples/common/ManagedMeshRenderer.h/.cpp` is shared with
`examples/UnityDemo` — see that file's header comment for why it lives
outside either engine's demo folder.
