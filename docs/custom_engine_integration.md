# Custom Engine Integration

```
Custom C++ engine (GameObject/Component)
        |
spatial::SpatialWorld — linked directly, no ABI boundary
```

Validated against an independent custom engine: **Phoenix Engine**
(`Anand-Ramnarain-27/Anand-PhoenixEngine`), a DirectX 12 engine with a
GameObject/Component architecture, deferred + forward rendering, and a
full ImGui editor — not a toy project built to make this integration
easy. The integration itself (`ComponentSpatialWorld`,
`PhoenixSpatialRenderer`, and the handful of engine-file edits below)
lives in that repository, not this one; this document describes the
pattern in general terms and points at the result.

## Direct linkage, not a C ABI — and why that's not automatic

Both `examples/UnityDemo` and `examples/UnrealDemo` wrap `SpatialWorld`
behind a flat C ABI. A custom C++ engine doesn't have Unity's P/Invoke
boundary forcing that, so direct linkage — the consuming engine links
`spatial_sdk.lib` and calls `spatial::SpatialWorld` C++ classes directly,
no marshaling layer — is the obvious choice, but only a safe one if it's
actually checked: direct linkage is only ABI-safe if the consumer's
toolset, runtime-library linkage (`/MD` vs `/MT`, Debug vs Release
matching exactly), and C++ standard match whatever built
`spatial_sdk.lib` — a mismatch there is undefined behavior at every
`std::string`/`std::vector` crossing the boundary (the exact risk
`examples/UnrealDemo`'s C ABI exists to avoid). For Phoenix Engine, this
was checked before writing any integration code: `Engine.vcxproj` targets
`v143` (VS2022) / C++20 / `MultiThreadedDLL` (`/MD`) Release,
`MultiThreadedDebugDLL` (`/MDd`) Debug, matching this repo's own CMake
build exactly. Had it not matched, a C ABI would have been the right
call here too.

`sdk/CMakeLists.txt`'s `SPATIAL_SDK_CUSTOM_ENGINE_STAGE_DIR` cache
variable (unset by default, never baked into this repo's default build)
copies the built static library and public headers into the consuming
engine's own third-party layout after each build, e.g.
`<PhoenixEngine>/Source/3rdParty/SpatialSDK/{include,lib/Debug,lib/Release}`
— the same role `examples/UnityDemo`/`UnrealDemo`'s native-plugin staging
plays, landing in an external repository instead of a folder this one
owns.

## Zero coordinate conversion

`spatial::core::Mat4` is right-handed, Y-up (see
`sdk/include/spatial/core/Mat4.h`). Phoenix Engine's math library
(Microsoft's DirectXTK SimpleMath) is also right-handed, Y-up
(`XMMatrixPerspectiveFovRH`/`XMMatrixLookAtRH` in `SimpleMath.inl`,
`Vector3::UnitY` as the look-at up vector in `ModuleCamera.cpp`), and its
rasterizer state (`GBufferPass.cpp`: `FrontCounterClockwise = TRUE`) uses
the same CCW-front-face convention the SDK's procedural mesh generator
produces. That means positions, normals, and triangle winding all cross
the boundary unmodified — no axis swap, no sign flip, no winding
reversal, unlike either engine plugin. Right-handed Y-up is a common
convention for custom/homebrew C++ engines built on OpenGL-style math (as
opposed to Unity's left-handed Y-up or Unreal's left-handed Z-up), and
it's part of why the SDK's core math picked that convention in Phase 2 —
this integration is the payoff of that choice.

## `PhoenixSpatialRenderer` — the simplest of the three `IRenderer`s

Because there's no ABI boundary, `PhoenixSpatialRenderer::createMesh()`
constructs the engine's own `Mesh` object directly (building its
`Vertex{position, texCoord, normal, tangent}` array and calling
`Mesh::setData(cmd, staticBuffer, ...)`) and `createMaterial()` constructs
the engine's own `Material` directly (`baseColor`/`metallic`/`roughness`
map almost 1:1 onto the SDK's own `data::Material` fields) — no
intermediate CPU-buffer pull step like `examples/common/ManagedMeshRenderer`
uses for the two C-ABI plugins. `drawMesh()` resolves straight to engine
pointers (`Mesh*`, `Material*`) at record time, not an opaque id needing
a later lookup.

One documented simplification: mesh GPU uploads go through the engine's
existing `ModuleStaticBuffer`, a bump allocator with no per-allocation
free. Streaming inherently creates and destroys many tiles over a
session; this allocator's pool is monotonically consumed, not reclaimed,
by that churn. Fine for a demo session, a real constraint for a
long-running one — flagged here rather than hidden.

## `ComponentSpatialWorld` — the one component an integration adds

Same shape as `SpatialWorldComponent`/`USpatialWorldComponent` in the
other two engines, expressed in Phoenix Engine's own idiom: a
`Component::Type` enum entry, registered in `ComponentFactory.cpp`,
exposing `onEditor()` (ImGui fields: dataset path, streaming radius,
memory budget, worker threads, debug/statistics toggles, a live stats
readout) and `onSave()`/`onLoad()` (the engine's existing hand-rolled JSON
component-serialization pattern, matching `ComponentDecal.cpp`'s
precedent).

One real architectural wrinkle, found by tracing the engine's actual
render path rather than trusting `Component::render()`'s signature:
**`Component::render(ID3D12GraphicsCommandList*)` is dead code** in this
engine (`ComponentMesh::render()`'s body is empty; the call site that
would invoke it is only reachable while editing a prefab). The real path
is a gather step — `EditorRenderer.cpp`'s `collectMeshes` lambda walks
the scene once per viewport per frame, reading `ComponentMesh::getEntries()`
(or, for non-asset-backed geometry, `Model::buildMeshEntries()`) into a
`std::vector<MeshEntry*>` handed to the G-buffer/forward passes.
`ComponentSpatialWorld::buildMeshEntries()` is the same shape as
`Model::buildMeshEntries()` — one line added to `collectMeshes` picks it
up, matching the existing branch for procedural models rather than
inventing a parallel rendering path.

Because that gather runs twice per frame (once for the Scene view, once
for the Game view, each with its own camera), `ComponentSpatialWorld`
guards `spatial::SpatialWorld::update()`/`render()` to run at most once
per real frame regardless of how many viewports ask — `LODManager`'s
hysteresis has per-frame timing state that would be double-advanced
otherwise. Both viewports read the same resulting draw list from one
authoritative camera (`ModuleCamera`, the same camera the Scene view
uses) — this integration doesn't attempt independent per-viewport LOD.

## Verification

`ComponentSpatialWorld`/`PhoenixSpatialRenderer` compile clean through
`MSBuild` against `Engine.vcxproj`, in both Debug and Release, x64.
Runtime behavior was confirmed by running the built `PhoenixEngine.exe`
and reading a file-based verification log (the engine's own logging
writes via `OutputDebugStringA`, which needs an attached debugger — this
integration adds its own file logger instead): streaming converged to
`resident=16 loading=0 requested=0 drawCommands=32` — the same numbers,
on the same generated dataset, as the Unity and Unreal demos — and held
steady for the full session with no errors. This matched what was
rendered on screen: solid, correctly shaded geometry within the debug
tile-bounds overlay, confirming the zero-coordinate-conversion result
above holds visually as well as by log.

## What's deliberately not here

- **No custom Inspector layout beyond `onEditor()`'s default field order**
  — matches the engine's own convention for every other component.
- **No per-allocation GPU buffer reclamation** — see the `ModuleStaticBuffer`
  note above.
- **No independent per-viewport LOD** — one authoritative camera drives
  streaming for both the Scene and Game views.
- **No frustum culling** — same limitation `SpatialWorld` already has in
  every integration.
