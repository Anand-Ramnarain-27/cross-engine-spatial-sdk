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
  +--> drain WorkerPool's completed loads
  |      - not in desired set anymore -> discard, count as a cancellation
  |      - succeeded                  -> LoadedCPU -> UploadPending -> Resident
  |      - failed                     -> Unloaded, count as a failure
  |
  +--> promote Requested -> Loading for anything a worker has picked up
  |
  +--> for tracked tiles no longer in the desired set:
  |      - Requested  -> cancel in the queue (free), -> Unloaded
  |      - Loading    -> leave alone; discarded on arrival above
  |      - Resident   -> UnloadRequested -> Unloading -> Unloaded
  |
  +--> issue new requests for newly-desired tiles (throttled per call)
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

## Known simplifications (Phase 6 scope)

- No memory/GPU budget or eviction policy yet — a tile leaving
  `streamingRadius` unloads immediately rather than lingering in an LRU
  cache. Phase 7 adds that on top of this baseline.
- A failed load is retried on the very next `update()` call (the tile
  re-enters the desired set and gets re-requested) with no backoff. Fine at
  this SDK's scale; a production system would rate-limit retries.
