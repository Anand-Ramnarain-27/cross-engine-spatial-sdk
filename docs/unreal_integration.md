# Unreal Integration

```
Unreal (C++, UActorComponent)
        |
SpatialUnrealPlugin.h (flat C ABI — direct extern "C" calls, no marshaling layer)
        |
spatial::SpatialWorld (C++, the same class StandaloneViewer and the Unity plugin use)
```

No core SDK logic lives in the Unreal-side C++. Streaming, LOD selection,
the tile resource state machine, and GPU-resource bookkeeping are all
`spatial::SpatialWorld` running inside `SpatialUnrealPlugin.dll`; the
Unreal-side code only marshals a camera in and geometry out, once per
tick.

## Why a second native plugin, not direct linkage

Unreal plugins are C++-native — unlike Unity, there's no P/Invoke boundary
forcing a C ABI. Linking `spatial_sdk.lib` straight into
`SpatialSDKPlugin`'s module and calling `spatial::SpatialWorld` directly
was the first option considered, and it's rejected for one concrete
reason: Unreal pins its own MSVC toolset and CRT settings per engine
release, and there's no guarantee they match whatever produced
`spatial_sdk.lib` in this repo's own CMake build. If they don't, every
`std::string`/`std::vector`/`std::variant` (all through `spatial::Expected<T>`
and friends) crossing that boundary is undefined behavior — heap
corruption that shows up nowhere near its actual cause, not a compile
error. A flat C ABI (`SpatialUnrealPlugin.h`, `extern "C"`, only
primitives/opaque-pointers/fixed-layout-structs cross it) sidesteps this
category of bug entirely, regardless of what compiled which side. This is
the same shape of decision `examples/UnityDemo` already made, for the
same reason, just there it's Unity's P/Invoke marshaler forcing the issue
rather than a choice.

This does mean a second, independent native plugin binary
(`SpatialUnrealPlugin.dll`, separate from `SpatialUnityPlugin.dll`) rather
than one shared binary both engines use — deliberate, not an oversight.
Real cross-engine SDKs ship a plugin binary per engine (different
packaging/versioning lifecycle per engine), and it matches this project's
own architecture diagram: Unity, Unreal, and a custom C++ renderer are
three separate boxes hanging off the same core, not one binary all three
share.

## `UProceduralMeshComponent`, not native RHI/scene-proxy interop

Same scope call as Unity's `Graphics.DrawMesh`, Unreal's equivalent:
`ManagedMeshRenderer` (shared with the Unity plugin — see
`examples/common/ManagedMeshRenderer.h`, genuinely engine-agnostic, never
touches a graphics API) records CPU-side draw commands and mesh data;
`USpatialWorldComponent` pulls them once per tick and keeps one
`UProceduralMeshComponent` per resident tile-mesh up to date. A
production integration would more likely go through Unreal's scene-proxy
/ RHI layer directly for performance; that's a materially larger,
separate project, flagged here the same way the managed-mesh choice is
flagged for Unity.

## Coordinate conversion — native-side this time, and why

`spatial::core::Mat4` is right-handed, Y-up, meters. Unreal is
left-handed, **Z-up**, **centimeters** — a bigger mismatch than Unity's
(an axis swap plus a 100x scale, not just a sign flip). The fix: swap the
Y and Z components of every position/direction/normal crossing the
boundary (positions additionally scaled ×100), and reverse each
triangle's winding to compensate — swapping exactly two axes is an odd
permutation, which by itself both converts Y-up to Z-up *and* flips
handedness with no separate sign negation needed (the same convention
glTF, itself RH/Y-up, uses when imported into Unreal). See
`UnrealCoordinateConversion.h`'s header comment for the full derivation,
including how a general (not just identity) transform matrix conjugates
through the same permutation.

Unlike Unity's plugin (where the equivalent conversion lives in
`CoordinateConversion.cs`, on the C# side), this conversion lives entirely
inside `SpatialUnrealPlugin.cpp` — every `SpatialUnreal_Get*` function
hands back data that's already Unreal-space. `USpatialWorldComponent` does
zero conversion math of its own. This is a deliberate refinement over the
Unity plugin's arrangement, made possible by the conversion needing to
live in *some* C++ binary either way (unlike Unity, there's no
host-language layer that couldn't also just be C++): putting it
native-side means the single most error-prone part of this boundary is
provable by this repo's own Catch2 suite
(`tests/examples/UnrealCoordinateConversionTests.cpp` — pure conversion
math; `SpatialUnrealPluginTests.cpp` — checks the conversion is actually
wired into the pulled mesh/camera data) instead of only by looking at the
screen. The Unity plugin isn't being retrofitted to match — it already
shipped, verified, and there's no bug driving a change there.

## The C ABI (`SpatialUnrealPlugin.h`)

Structurally the same shape as `SpatialUnityPlugin.h` (see
`docs/unity_integration.md`'s table — same function categories: lifecycle,
load, update/render, draw-command pull, mesh/material pull, debug-line
pull, statistics), with camera input and every geometry output already
Unreal-space. `SpatialUnrealLoadConfig::streamingRadiusCm` is explicitly
named in centimeters (converted to meters internally) so the unit isn't
ambiguous at the call site.

## `USpatialWorldComponent`

The one component an integration adds to an `AActor` — matches the
project brief's suggested class name exactly. Inspector-visible
`UPROPERTY`s: `DatasetPath` (relative to `<ProjectDir>/SpatialSDKData/`),
`StreamingRadiusCm`/`MaxResidentTiles`/`CpuMemoryBudgetMB`/
`WorkerThreadCount`/`MaxGPUUploadsPerUpdate` (passed straight through to
`SpatialWorldConfig`), `bEnableDebugVisualization` (drives `DrawDebugLine`
calls, one-frame lifetime, redrawn every tick), `bEnableStatistics`
(drives a `GEngine->AddOnScreenDebugMessage` overlay with stable keys, the
Unreal equivalent of the Unity plugin's `OnGUI` block), `TileMaterial`
(optional — if unset, `UProceduralMeshComponent` falls back to Unreal's
own default material rather than going uncolored/unlit), and
`CameraOverride` (defaults to the first local player's camera manager).
`DatasetMaxLOD` (the brief's "Maximum LOD") is `BlueprintReadOnly` for the
same reason it's read-only in the Unity plugin: it's dataset-derived
information, not a runtime cap the SDK has a concept of.

Per-tile mesh components are pooled by `meshId` and hidden (not
destroyed) when a tile stops being drawn — geometry for a still-resident
tile never changes, so there's no reason to rebuild a
`UProceduralMeshComponent` every tick, and a tile that drops out of
streaming radius and later comes back reuses the same component. Per-tile
color needs `TileMaterial` to expose a `BaseColor` vector parameter;
setting it on a material without one is a harmless no-op (Unreal's
default behavior for an unrecognized parameter name), not an error — so
an unconfigured `TileMaterial` still renders solid and lit, just
uncolored.

## Project layout

```
examples/UnrealDemo/
├── NativePlugin/                          C++ CMake target -> SpatialUnrealPlugin.dll
│   └── src/
│       ├── SpatialUnrealPlugin.h/.cpp      the C ABI, with native coordinate conversion
│       └── UnrealCoordinateConversion.h    the one place handedness/units are handled
└── UnrealProject/                          a real, openable Unreal project (UE 5.6)
    ├── SpatialSDKDemo.uproject
    ├── Source/SpatialSDKDemo/              minimal primary game module (makes this a C++ project)
    ├── Plugins/SpatialSDKPlugin/           the actual plugin
    │   ├── SpatialSDKPlugin.uplugin
    │   └── Source/
    │       ├── SpatialSDKPlugin/           USpatialWorldComponent + module glue
    │       └── ThirdParty/SpatialUnrealPlugin/   staged DLL/.lib/header (gitignored, build artifact)
    └── SpatialSDKData/DemoCity/             demo dataset (gitignored, regenerate — see examples/UnrealDemo/README.md)
```

`examples/common/ManagedMeshRenderer.h/.cpp` is shared with
`examples/UnityDemo` — see that file's header comment for why it lives
outside either engine's demo folder.

## What's deliberately not here

- **No native RHI/scene-proxy interop** — see the scope decision above.
- **No frustum culling** — same limitation `SpatialWorld` already has.
- **No custom Details-panel customization** — `USpatialWorldComponent`'s
  `UPROPERTY`s use Unreal's default auto-generated Details panel.
- **No automatic per-tile material** — a working demo material with a
  `BaseColor` parameter isn't shipped; `TileMaterial` left unset falls
  back to Unreal's default material (solid, lit, uncolored) rather than
  silently failing.
