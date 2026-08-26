# Public SDK API

The class every engine integration is meant to build on is
`spatial::SpatialWorld` (`sdk/include/spatial/SpatialWorld.h`). Everything
else in the SDK (streaming, LOD, rendering, debug visualization) is public
and independently usable, but `SpatialWorld` is the one object that composes
them so an integration doesn't have to. See `docs/architecture.md`'s
"`SpatialWorld` — the public API façade" section for how it's built and why
it looks the way it does; this page is the API reference and usage guide.

## Minimal usage

```cpp
using namespace spatial;

SpatialWorld world;
SpatialWorldConfig config{};
config.streaming.streamingRadius = 400.0f;
config.streaming.memoryBudget.maxResidentTiles = 256;

if (const Expected<void> result = world.loadDataset("city.world", config); !result.hasValue())
{
    // result.error().code, result.error().message
}

// Once per frame, with `renderer` implementing spatial::rendering::IRenderer:
world.update(cameraParams, renderer);   // advances streaming, processes GPU uploads

renderer.beginFrame(viewProjectionMatrix);
world.render(renderer, cameraParams);   // draws resident tiles + debug overlay
renderer.endFrame();
renderer.present();                     // however the backend presents a frame

world.shutdown();                       // or just let `world` go out of scope
```

This is exactly what `examples/StandaloneViewer/src/main.cpp` does, with a
real window and a real `D3D11Renderer` around it.

## `SpatialWorldConfig`

| Field | Meaning |
|---|---|
| `streaming` (`streaming::StreamingConfig`) | Streaming radius, worker thread count, priority weights, memory budget. See `docs/streaming.md`. |
| `lod` (`lod::LODConfig`) | Distance-vs-screen-space-error mode, hysteresis. See `docs/lod.md`. |
| `tilesDirectory` | Where to find `.tile` files. Defaults to `<manifest dir>/tiles` (the convention `SpatialTileBuilder` writes to) if left unset. |
| `maxGPUUploadsPerUpdate` | Caps GPU uploads processed per `update()` call, so a burst of newly-resident tiles can't stall a frame. Default `8`. |
| `debugVisualizationEnabled` | Whether `render()` draws the color-coded tile-bounds overlay. Toggleable at runtime via `setDebugVisualizationEnabled()`. |

## `SpatialWorld` methods

| Method | Notes |
|---|---|
| `Expected<void> loadDataset(path, config = {})` | Loads a manifest, builds the spatial index, and (re)starts streaming around it. Safe to call again to switch datasets: on success the previous dataset and its GPU resources are released first; on failure, a previously-loaded dataset is left untouched. |
| `void shutdown()` | Releases the loaded dataset, streaming state, and every GPU resource currently held. The `IRenderer` those resources were created with must still be valid when this runs — see the destruction-order note below. |
| `void update(camera, renderer)` | Call once per frame: advances streaming and processes up to `maxGPUUploadsPerUpdate` pending uploads. |
| `void render(renderer, camera)` | Draws every GPU-ready resident tile at its selected LOD, then the debug overlay if enabled. Call between the caller's own `renderer.beginFrame()`/`endFrame()` — this method calls neither. |
| `bool isLoaded() const` | |
| `const DatasetManifest& datasetManifest() const` | Only valid when `isLoaded()`. |
| `StreamingStatistics statistics() const` | Resident/loading/requested counts, memory usage, cache hits, etc. — see `docs/streaming.md`. |
| `bool debugVisualizationEnabled() const` / `setDebugVisualizationEnabled(bool)` | Runtime toggle (bound to `F1` in `StandaloneViewer`). |

## A lifetime rule that matters

`SpatialWorld`'s GPU resources (`rendering::MeshResource`/`MaterialResource`,
wrapped in `GPUResource<HandleT, Destroy>` — see `docs/rendering.md`) hold
pointers into whatever `IRenderer` created them. **Declare/construct your
`SpatialWorld` after your `IRenderer`, so it's destroyed first** (C++
destroys locals in reverse declaration order) — on every exit path,
including an exception unwinding the scope, not just the one you tested by
hand. Getting this backwards compiles fine and only crashes at teardown;
see `docs/architecture.md`'s Phase 9 section for the two times this bug was
actually hit (once in `main.cpp`, once independently reintroduced in the
test suite) while building this class.

## What's deliberately not here yet

- No frustum-based visibility filtering at the `SpatialWorld` level —
  `render()` draws every GPU-ready resident tile; `update()`'s desired-tile
  determination is radius-based, not frustum-based (see `docs/streaming.md`
  for why). Actual frustum culling of draw calls is future work.
- No multi-renderer / multi-viewport support — one `SpatialWorld` assumes
  one `IRenderer` for its GPU resource lifetime per load. Nothing prevents
  calling `update()`/`render()` with a different renderer after a
  `loadDataset()`/`shutdown()` cycle, but resources aren't shared across
  renderers within one loaded dataset.
