# Streaming

`spatial::streaming` (`sdk/include/spatial/streaming/`) is the core
milestone: `StreamingManager` decides which tiles should be resident each
frame, requests missing ones, cancels/unloads ones no longer needed, and
drives them through an explicit, validated resource state machine. All of
it is main-thread-callable; the actual file I/O happens on worker threads.

## State machine

```
Unloaded -> Requested -> Loading -> LoadedCPU -> UploadPending -> Resident -> UnloadRequested -> Unloading -> Unloaded
```

`ResourceState.h` defines this as data (`isValidTransition(from, to)`), and
`StreamingManager::transition()` asserts every transition against it — an
invalid transition is a programming error caught immediately in a debug
build, not a silent state corruption.

`UploadPending` exists because the full state machine includes the GPU
upload step Phase 8 (Rendering) will add. Until then, there is no GPU
resource yet, so `StreamingManager` passes a successfully-loaded tile
through `LoadedCPU -> UploadPending -> Resident` in one step, within the
same `update()` call. Phase 8 only needs to make that middle step
asynchronous and GPU-driven — the state machine, and everything that reads
`ResourceState`, doesn't change.

## Per-frame flow

```
StreamingManager::update(camera)
  |
  +--> TileIndex::queryRadius(camera.position, streamingRadius)   -> desired set
  |
  +--> count cache hits + touch recency for desired tiles already Resident
  |
  +--> drain WorkerPool's completed loads
  |      - not in desired set anymore -> discard, count as a cancellation
  |      - succeeded                  -> LoadedCPU -> UploadPending -> Resident,
  |                                       data handed to TileCache::put()
  |      - failed                     -> Unloaded, count as a failure
  |
  +--> promote Requested -> Loading for anything a worker has picked up
  |
  +--> for Requested/Loading tiles no longer in the desired set:
  |      - Requested  -> cancel in the queue (free), -> Unloaded
  |      - Loading    -> leave alone; discarded on arrival above
  |      (Resident tiles are untouched here — see Cache below)
  |
  +--> TileCache::evictToBudget(desired set)   -> evicted tiles:
  |      Resident -> UnloadRequested -> Unloading -> Unloaded
  |
  +--> issue new requests for newly-desired, not-yet-tracked tiles (throttled per call)
```

Desired tiles come from `TileIndex::queryRadius` alone — not frustum
intersection. Turning the camera shouldn't cause nearby-but-currently-
offscreen tiles to unload and immediately reload; that's what the
`streamingRadius` preload margin is for. Frustum visibility instead feeds
into *priority* (see below), and separately drives what actually gets
drawn once Phase 8's renderer exists.

## Cancellation

Cancelling a request costs nothing extra only while it's still `Requested`
— `RequestQueue::cancel()` removes it before a worker ever sees it. A
request already `Loading` can't be interrupted (this SDK's tile loads are
small, synchronous file reads — not worth adding interruptible I/O for);
instead it's allowed to finish and its result is silently discarded on
arrival if the tile is no longer desired by then. This is what the project
brief's "cancelled when safe" means in practice: never *starting* wasted
work is free, but work already in flight is left to complete.

## Priority

```
priority = distancePriorityWeight * distanceScore + directionPriorityWeight * directionScore
```

- `distanceScore` — `1 - clamp(distance / streamingRadius, 0, 1)`; closer is
  higher.
- `directionScore` — maps the dot product between the camera's forward
  vector and the direction to the tile from `[-1, 1]` to `[0, 1]`; tiles
  ahead of the camera score higher than tiles behind it. This stands in for
  "visibility" without needing a full `Frustum` at the streaming layer (see
  `docs/architecture.md`'s Core layering — `CameraParams` deliberately
  doesn't carry enough to build one; that's a render-time construct).

"LOD importance" from the project brief's factor list isn't a separate
term: since closer tiles are both higher streaming priority *and* selected
at a finer LOD once resident (see `docs/lod.md`), adding a redundant
explicit term would double-count the same signal.

`RequestQueue` is a `std::priority_queue` ordered by this score; on pop, a
request cancelled after being pushed is silently skipped (lazy deletion)
rather than physically removed from the heap.

## Threading

`WorkerPool` owns a fixed-size thread pool and the `RequestQueue` they pull
from. Workers only ever call the injected `TileLoader` and push results to
a mutex-guarded completed list — `StreamingManager::update()` drains that
list on the main thread, so no tile data or GPU-adjacent state crosses
threads without going through it. `makeFileTileLoader()` builds the default
disk-based loader (`TileSerializer::loadTile` under a tiles directory);
tests inject their own in-memory or artificially-delayed loaders instead
(see `tests/streaming/StreamingManagerTests.cpp`'s `ControllableLoader`),
which is what makes priority ordering and cancellation timing testable
deterministically rather than via sleep-and-hope.

## Cache and memory budget (Phase 7)

A tile leaving `streamingRadius` is **not** unloaded on the spot — it stays
Resident, held by `TileCache`, in case the camera pans back. `TileCache`
owns the actual `data::Tile` CPU data once a load completes (`StreamingManager`
only tracks `ResourceState` after that point); `StreamingManager::residentTile()`
reads through to it.

Eviction runs once per `update()`, evicting the lowest-scoring cached tiles
*not currently desired* until every budget is satisfied:

- `cpuBudgetBytes` — checked against `TileMemory.h`'s
  `estimateTileMemoryBytes()` (vertex/index buffers dominate; a coarse,
  deliberately inexact estimate — allocator overhead isn't worth modeling).
- `gpuBudgetBytes` — an abstraction for now: mirrors the CPU estimate,
  since no real GPU resource exists until Phase 8 gives it something to
  measure.
- `maxResidentTiles` — a simple count cap.

**Eviction strategy — combined score, not pure LRU or pure priority:**

```
keepScore = lastPriority - recencyWeight * framesSinceLastTouched
```

`lastPriority` is the same distance/direction score requests are queued by
(see above); `framesSinceLastTouched` grows every `update()` a tile isn't
in the desired set. The lowest `keepScore` is evicted first. This folds the
project brief's "LRU", "distance/priority", and "combined" eviction
strategy options into one tunable formula: `recencyWeight` near 0 makes
eviction purely priority-driven (always keep the closest tiles regardless
of how long they've sat unused); a larger `recencyWeight` makes it behave
more like LRU (an old low-value tile loses out to a recently-relevant one
even if their priorities are similar). Default `0.01` needs roughly 100
untouched frames to fully offset one point of priority.

Tiles in the current frame's desired set are never evicted, even if that
means briefly exceeding budget — the budget governs the *cache* of
retained-but-currently-unneeded tiles, not what's actively required this
frame. `StreamingStatistics::totalCacheHits` counts desired tiles that were
already Resident when `update()` started, i.e. reused without a reload —
`totalCacheHits / (totalCacheHits + totalLoadsCompleted)` is the "Cache
Hits: 87%" figure from the project brief's debug overlay mockup.

## Known simplifications (Phase 6/7 scope)

- A failed load is retried on the very next `update()` call (the tile
  re-enters the desired set and gets re-requested) with no backoff. Fine at
  this SDK's scale; a production system would rate-limit retries.
- The GPU budget is a placeholder number, not a real measurement — see
  above. Phase 8 gives it something real to track.
