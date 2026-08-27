# Profiling

`spatial::debug::Profiler` (`sdk/include/spatial/debug/Profiler.h`) is
lightweight, always-on CPU timing built into `SpatialWorld` itself, not a
separate opt-in tool. Every `update()`/`render()` call records how long its
major internal steps took, and the result is available immediately through
`SpatialWorld::frameProfile()` — no external profiler, build flag, or
symbol server required to see where a frame's time goes.

## Why this lives in `spatial::debug`

`docs/architecture.md`'s module map calls out "statistics, profiling
instrumentation" as part of the Debug layer from the very first design
pass, alongside `DebugRenderer`. Both exist for the same reason: an
engine integration needs to see what the SDK is doing, not just trust
that it's doing the right thing. `Profiler` is not a template and not
engine-agnostic in the abstract sense — it measures a fixed, SDK-specific
set of sections, matching the project's general preference for concrete
code over speculative generality.

## `ProfileSection`

```cpp
enum class ProfileSection : std::uint8_t
{
    StreamingUpdate,   // StreamingManager::update() — request/eviction bookkeeping
    GPUUpload,         // GPUUploadQueue::processQueue() — draining pending uploads
    LODSelection,       // per-tile LOD selection + issuing draw calls
    DebugDraw,          // debug overlay: tile-bounds gathering + flush()
    Count,
};
```

`FrameProfile` holds one `double` (milliseconds) per section plus a
`totalMs` covering the whole `update()` + `render()` pair. `section()`
looks a value up by enum value instead of by index.

## How it's wired into `SpatialWorld`

`update()` calls `m_profiler.beginFrame()` first, then wraps the
streaming-manager update and the GPU upload drain in scoped timers:

```cpp
{
    const auto section = m_profiler.measure(debug::ProfileSection::StreamingUpdate);
    m_streamingManager->update(camera);
}
...
{
    const auto section = m_profiler.measure(debug::ProfileSection::GPUUpload);
    m_uploadQueue.processQueue(renderer, m_config.maxGPUUploadsPerUpdate);
}
```

`render()` does the same around the per-tile LOD-selection/draw loop and
the debug-overlay block, then calls `m_profiler.endFrame()` on every exit
path — including the early returns for "no dataset loaded" and "debug
visualization disabled" — so `totalMs` and every section always reflect
a complete, consistent frame rather than a partially-measured one.

`ScopedSection` is a small RAII timer (`std::chrono::steady_clock`): its
destructor adds the elapsed time to the profiler's current frame, so a
section's cost is measured by scope rather than by manual start/stop
calls that could be forgotten on an early return.

## Reading the result

```cpp
world.update(camera, renderer);
world.render(renderer, camera);

const spatial::debug::FrameProfile& profile = world.frameProfile();
std::cout << "total: " << profile.totalMs << "ms "
          << "(stream " << profile.section(spatial::debug::ProfileSection::StreamingUpdate) << "ms, "
          << "upload " << profile.section(spatial::debug::ProfileSection::GPUUpload) << "ms, "
          << "lod " << profile.section(spatial::debug::ProfileSection::LODSelection) << "ms, "
          << "debug " << profile.section(spatial::debug::ProfileSection::DebugDraw) << "ms)\n";
```

`frameProfile()` always returns the most recently completed frame — before
the first `update()`/`render()` call it's zero-initialized, matching
`SpatialWorld`'s general pattern of returning a valid default rather than
requiring a null check.

## `StandaloneViewer --profile-csv <path>`

The viewer can log every frame's timing to a CSV file for offline
inspection:

```
frame,elapsedSeconds,streamingUpdateMs,gpuUploadMs,lodSelectionMs,debugDrawMs,totalMs,residentTiles,loadingTiles,requestedTiles
```

Running it against the 1024-tile `BigCity` dataset shows the expected
shape: frame 0 carries a load-time spike (multiple milliseconds of
`StreamingUpdate` and `DebugDraw` as the first batch of tiles is
requested and drawn), the following frames are dominated by `GPUUpload`
as mesh/material uploads drain, and once streaming converges
(`loadingTiles == 0`, `requestedTiles == 0`) the frame settles to roughly
1–2ms total, now dominated by `LODSelection` — the recurring per-frame
cost of walking resident tiles and selecting a LOD, rather than any
one-time loading cost.

The title bar also shows a live one-line breakdown
(`Frame 1.75ms | Stream 0.23/Upload 0.00/LOD 1.15/Debug 0.29`) whether or
not `--profile-csv` is set, so the four section costs are visible without
needing to open the CSV at all.

## Known simplifications

- CPU timing only — no GPU timestamp queries, so time actually spent on
  the GPU executing a draw call isn't captured, only the CPU-side cost of
  issuing it.
- One frame of history (`lastFrame()`) — no rolling average, min/max, or
  percentile tracking. A CSV export plus offline analysis (as
  `--profile-csv` demonstrates) is the intended way to look at trends
  over time rather than building that into the SDK itself.
- Fixed section list. Adding a new measured section means extending
  `ProfileSection` and instrumenting the corresponding code path, not
  registering an arbitrary named section at runtime — consistent with
  the project's general preference against speculative generality.
