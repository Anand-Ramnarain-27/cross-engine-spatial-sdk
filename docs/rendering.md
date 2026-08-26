# Rendering

`spatial::rendering` (`sdk/include/spatial/rendering/`) is the SDK's only
point of contact with a GPU — and, by design, it never actually touches
one. Everything in this module is a pure interface, an RAII wrapper, or a
bounded queue; the real graphics-API calls live entirely in a backend that
implements `IRenderer`. This phase does not include a working backend —
that's Phase 9 (Standalone Viewer), which is also where a real window/GPU
context first exists. Everything here is verified against a recording
`MockRenderer` test double instead.

## `IRenderer`

```cpp
class IRenderer
{
public:
    virtual void beginFrame(const core::Mat4& viewProjection) = 0;
    virtual void endFrame() = 0;

    virtual MeshHandle createMesh(const data::Mesh& mesh) = 0;
    virtual void destroyMesh(MeshHandle handle) = 0;

    virtual MaterialHandle createMaterial(const data::Material& material) = 0;
    virtual void destroyMaterial(MaterialHandle handle) = 0;

    virtual TextureHandle createTexture(std::span<const std::byte> pixels, std::uint32_t width, std::uint32_t height) = 0;
    virtual void destroyTexture(TextureHandle handle) = 0;

    virtual void drawMesh(MeshHandle mesh, MaterialHandle material, const core::Mat4& worldTransform) = 0;
    virtual void drawDebugLines(std::span<const DebugVertex> vertices) = 0;
};
```

No render passes, no pipeline state objects, no shader management — just
enough surface for the SDK to hand a backend CPU data and get a handle
back, and to submit draws. `createTexture` exists for interface
completeness (`docs/tile_format.md`'s "known simplifications" notes
`Material` carries no texture references yet), so it's currently only
exercised by tests with synthetic pixel data.

`MeshHandle`/`TextureHandle`/`MaterialHandle` are phantom-typed opaque IDs
(`Handle<Tag>` in `ResourceHandle.h`) — same representation, but a
`MeshHandle` can't be passed where a `MaterialHandle` is expected, and it
costs nothing at runtime.

## RAII ownership

`GPUResource<HandleT, Destroy>` (`GPUResource.h`) is a move-only wrapper
that calls the right `IRenderer::destroy*` when it goes out of scope —
`MeshResource`, `MaterialResource`, and `TextureResource` are aliases of it
over a non-type template parameter (a pointer to the matching destroy
member function), so the three don't need three copies of the same RAII
logic. This is what "resource lifetime management" means concretely here:
nothing has to remember to call `destroyMesh` — a `MeshResource` going out
of scope, being reset, or being replaced by a move-assignment does it
automatically, exactly once.

## GPU upload queue

`GPUUploadQueue::processQueue(renderer, maxUploads)` uploads at most
`maxUploads` pending mesh/material requests per call and invokes each
one's callback with the resulting RAII resource. This is what finally
makes `ResourceState::UploadPending` a real, boundable step instead of the
`StreamingManager`-internal pass-through it's been since Phase 6 — Phase 9
wires a `StreamingManager` up to this queue and a real `IRenderer` so
`LoadedCPU -> UploadPending -> Resident` genuinely spans a frame (or more)
instead of completing instantly.

**Why `GPUUploadQueue` isn't already wired into `StreamingManager`:**
`docs/architecture.md`'s dependency graph has Streaming and Rendering as
sibling branches under the API layer — Streaming doesn't depend on
Rendering (or vice versa). Wiring them together is exactly the kind of
cross-module orchestration the API layer (Phase 9's viewer, and eventually
`SpatialWorld`) exists to do; `StreamingManager` gaining an `IRenderer`
dependency directly would break that boundary for no benefit, since
nothing before Phase 9 has a real renderer to hand it anyway.
`tests/rendering/StreamingRenderingIntegrationTests.cpp` proves the two
modules compose correctly today, without either depending on the other:
it pulls a real resident tile out of a `StreamingManager` and pushes its
meshes through a `GPUUploadQueue`.

Not thread-safe by design — GPU calls only ever happen on the thread that
owns the render context, so `enqueue*`/`processQueue` are both meant to be
called from that one thread only.

## Debug rendering

`spatial::debug::DebugRenderer` (`sdk/include/spatial/debug/`) accumulates
line geometry (`drawLine`, `drawAABB`, `drawTileBounds`) and submits it to
an `IRenderer` in one batched `drawDebugLines` call via `flush()`, rather
than one draw call per box — a debug overlay for a world with thousands of
tiles should not cost thousands of draw calls.

`colorForState()` implements the project brief's legend exactly:

```
[GREEN]   Resident
[YELLOW]  Loading / LoadedCPU / UploadPending (in transit)
[RED]     Requested
[GRAY]    Unloaded / UnloadRequested / Unloading
```

`Debug` is allowed to depend on `Streaming` (for `ResourceState`) and
`Rendering` (to actually draw) per the architecture doc's layering — it's
the one module that's explicitly allowed to reach across, since a debug
overlay's entire job is to visualize what other modules are doing.

## Known simplifications (Phase 8 scope)

- No real backend yet. `IRenderer` is fully specified and tested, but the
  only implementation that exists is `MockRenderer` in `tests/rendering/` —
  a recording test double, not something a real application could render
  with. Phase 9 builds the first real one.
- No frustum-drawing debug primitive (`drawFrustum`) yet — reconstructing a
  frustum's corner points from its six planes needs plane-triple
  intersection, or storing/deriving an inverse view-projection matrix
  (`Mat4` has no `inverse()` yet). Deferred until Phase 9 has an actual
  camera to source the corners from directly.
