# Unity Integration

```
Unity (C#, MonoBehaviour)
        |
SpatialWorldNative (C#, [DllImport] P/Invoke)
        |
SpatialUnityPlugin (C ABI, examples/UnityDemo/NativePlugin)
        |
spatial::SpatialWorld (C++, the same class StandaloneViewer uses)
```

No core SDK logic lives in C#. Streaming, LOD selection, the tile resource
state machine, and GPU-resource bookkeeping are all `spatial::SpatialWorld`
running in `SpatialUnityPlugin.dll`; the C# side only marshals a camera in
and geometry out, once per frame.

## The scope decision: managed meshes, not a native graphics plugin

A Unity native plugin can integrate at two very different levels:

1. **Low-level native rendering plugin API** (`IUnityGraphicsD3D11`,
   `GL.IssuePluginEvent`) — the native plugin creates GPU resources
   directly on Unity's own graphics device and submits draw calls from
   inside Unity's render thread. This is how you'd build a
   production-grade integration, and it's genuinely a separate project:
   the interop differs across Built-in/URP/HDRP and across graphics APIs,
   and getting it wrong tends to fail silently or on driver-specific
   timing rather than at compile time.
2. **Managed mesh bridge** (what's implemented here) — the native plugin
   never touches a graphics API. `IRenderer::createMesh` /
   `drawMesh` / `drawDebugLines` just record CPU-side data; Unity pulls it
   across the C API once per frame and turns it into real
   `UnityEngine.Mesh` objects drawn with `Graphics.DrawMesh`, and real `GL`
   immediate-mode lines for the debug overlay.

(2) is what this repo builds. It's a legitimate, commonly used pattern for
native streaming plugins (procedural/streamed-terrain and photogrammetry
importers do the same handoff), it works unmodified under Built-in, URP,
or HDRP since it never assumes a graphics API, and it keeps the same
`IRenderer` boundary the SDK already had — `ManagedMeshRenderer` is just
another `IRenderer` implementation, the same relationship `D3D11Renderer`
has to `IRenderer` in `StandaloneViewer`. The tradeoff: an extra CPU->CPU
copy per new mesh (once, cached after) and per frame's draw list, and no
control over exactly when Unity's render thread consumes what's drawn.
Neither matters at the tile counts this SDK targets; both would need
revisiting for a production integration at a much larger scale.

## Coordinate conversion

`spatial::core::Mat4` is right-handed, Y-up (`sdk/include/spatial/core/Mat4.h`).
Unity is left-handed, Y-up. Same up axis, opposite handedness — the fix
(the same one glTF-for-Unity importers use) is to negate Z on every
position/direction/normal crossing the native<->C# boundary and reverse
each triangle's winding to compensate (negating one axis mirrors the mesh,
which flips front-face winding; reversing indices restores it). This lives
in exactly one place:
`UnityProject/Assets/SpatialSDK/Scripts/Native/CoordinateConversion.cs`.
It's the same category of bug as Phase 9's HLSL row-major/`mul()` mismatch
— a convention difference at an engine boundary that compiles fine and
only shows up as visibly wrong geometry — so it gets the same treatment:
one conversion point, documented inline, verified by actually looking at
the result in the Unity Editor rather than assumed correct.

## The C ABI (`SpatialUnityPlugin.h`)

Unity's P/Invoke marshaler can only cross the boundary with primitives,
opaque pointers, and fixed-layout (`[StructLayout(LayoutKind.Sequential)]`)
structs — it cannot call a C++ member function or see a C++ class layout.
Every operation is therefore a flat `extern "C"` function taking an opaque
`SpatialUnityWorldHandle`:

| Function | Purpose |
|---|---|
| `SpatialUnity_CreateWorld` / `_DestroyWorld` | Lifecycle for one `spatial::SpatialWorld` + its `ManagedMeshRenderer`. |
| `SpatialUnity_LoadDataset` | Wraps `SpatialWorld::loadDataset`; returns a `SpatialUnityResult` (0 = ok, else `ErrorCode`'s ordinal + 1). |
| `SpatialUnity_Update` / `_Render` | Called once per frame each, mirroring `SpatialWorld::update()`/`render()`. `_Render` also brackets the call in `ManagedMeshRenderer::beginFrame()`/`endFrame()`, which is what clears the previous frame's draw/debug-line lists. |
| `SpatialUnity_GetDrawCommandCount` / `_GetDrawCommands` | Pulls this frame's `{meshId, materialId, transform}` list. |
| `SpatialUnity_GetMeshVertexCount` / `_GetMeshIndexCount` / `_GetMeshData` | Pulls one mesh's geometry by id — called once per id, then cached C#-side, since mesh data doesn't change while a tile stays resident. |
| `SpatialUnity_GetMaterialColor` | Pulls one material's base color by id. |
| `SpatialUnity_GetDebugLineVertexCount` / `_GetDebugLineData` | Pulls this frame's debug tile-bounds line list, when visualization is enabled. |
| `SpatialUnity_GetStatistics` / `_GetDatasetMaxLOD` / `_IsLoaded` / `_Set/GetDebugVisualization` | Everything `SpatialWorldComponent`'s stats overlay and Inspector toggles need. |

Every geometry-pulling function follows the same caller-allocates pattern:
C# calls a `*Count` function first, allocates a managed array of the right
size, then calls the corresponding `*Data` function to fill it — no
`unsafe` code, no native-side allocation for C# to free.

`SpatialUnityPlugin.h`'s comment states plainly that
`SpatialWorldNative.cs` must be kept in sync by hand, field for field —
there's no codegen step. `tests/examples/SpatialUnityPluginTests.cpp`
exercises the actual exported functions (not just the C++ classes behind
them) precisely because that boundary — struct layout, caller-allocated
buffers, id-not-found behavior — is exactly what a C++-only test would
miss.

## `SpatialWorldComponent`

The one `MonoBehaviour` an integration adds to a `GameObject`. Inspector
fields, matching the project brief's suggested list:

| Field | Notes |
|---|---|
| `datasetPath` | Relative to `Assets/StreamingAssets/SpatialSDK/`. |
| `streamingRadius`, `maxResidentTiles`, `cpuMemoryBudgetMB`, `workerThreadCount`, `maxGPUUploadsPerUpdate` | Passed straight through to `SpatialWorldConfig`/`StreamingConfig`. |
| `enableDebugVisualization` | Runtime-toggleable; also drives the `OnRenderObject()` GL line pass. |
| `enableStatistics` | Runtime `OnGUI()` overlay: resident/loading/requested counts, CPU/GPU memory, cache hits, dataset max LOD. |
| `material` | Shared material tiles are drawn with; per-tile base color comes from the SDK material via a `MaterialPropertyBlock` (both `_BaseColor` and `_Color` are set, so the same procedural color works whether `material` is a Built-in Standard material or a URP/HDRP Lit one). |
| `cameraOverride` | Defaults to `Camera.main` if left unset. |

`DatasetMaxLOD` ("Maximum LOD" in the brief's property list) is exposed
read-only, populated once `loadDataset` succeeds, and shown in the stats
overlay rather than as an editable field — it's dataset-derived
information (how many LOD levels `SpatialTileBuilder` generated), not a
runtime cap the SDK has any concept of. A field that looked editable but
didn't do anything would be worse than not having it.

Lifetime follows the same rule documented in `docs/sdk_api.md`:
`OnDestroy()` calls `SpatialUnity_Shutdown` (releases every GPU-side
record `ManagedMeshRenderer` tracks) before `SpatialUnity_DestroyWorld`
(frees the renderer itself) — same ordering as `StandaloneViewer`'s
renderer/world declaration order, just expressed as two explicit calls
instead of C++ destruction order since there's no equivalent in C#.

## Project layout

```
examples/UnityDemo/
├── NativePlugin/                        C++ CMake target -> SpatialUnityPlugin.dll
│   └── src/
│       ├── SpatialUnityPlugin.h/.cpp     the C ABI
│       └── ManagedMeshRenderer.h/.cpp    the IRenderer implementation
└── UnityProject/                         a real, openable Unity project
    └── Assets/
        ├── SpatialSDK/Scripts/
        │   ├── Native/SpatialWorldNative.cs        [DllImport] bindings
        │   ├── Native/CoordinateConversion.cs       the one place handedness is handled
        │   └── SpatialWorldComponent.cs             the MonoBehaviour
        ├── Plugins/x86_64/                          SpatialUnityPlugin.dll (build artifact, gitignored)
        ├── StreamingAssets/SpatialSDK/DemoCity/      demo dataset (gitignored, regenerate below)
        └── Scenes/SpatialSDKDemo.unity
```

Building `SpatialUnityPlugin` (part of the normal top-level CMake build,
gated by `SPATIAL_SDK_BUILD_EXAMPLES` like `StandaloneViewer`) copies the
DLL straight into `Assets/Plugins/x86_64/` as a post-build step, so
rebuilding after a core SDK change and reopening/re-entering Play mode in
the Editor picks it up without any manual copying.

The demo dataset isn't committed (same policy as `assets/datasets/` —
generated, reproducible, not source). Regenerate it with:

```bash
SpatialTileBuilder --output examples/UnityDemo/UnityProject/Assets/StreamingAssets/SpatialSDK/DemoCity --name DemoCity --grid 4 --tile-size 50 --max-lod 2 --buildings-per-tile 3 --seed 42
```

## What's deliberately not here

- **No native graphics-API interop** — see the scope decision above.
- **No frustum culling / occlusion** — same limitation `SpatialWorld`
  already has (see `docs/sdk_api.md`); every GPU-ready resident tile is
  drawn every frame regardless of whether the camera can see it.
- **No custom Inspector/PropertyDrawer** — `SpatialWorldComponent`'s
  fields use Unity's default Inspector; a hand-built one would mostly
  duplicate it for a field list this short.
