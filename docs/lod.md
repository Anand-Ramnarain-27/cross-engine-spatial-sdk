# Level of Detail

`spatial::lod` (`sdk/include/spatial/lod/`) selects a per-tile LOD index
from camera distance, with hysteresis to prevent flicker at a boundary.
Header-only, templated on an opaque key (see "Why generic over a key"
below), and independent of streaming — see "Why LOD doesn't drive
streaming" below.

## Two selection modes, one algorithm

- **Distance mode** (`selectLODByDistance`, `DistanceLOD.h`): a plain
  ascending list of distance thresholds. `thresholds[i]` is the boundary
  between LOD `i` and LOD `i+1`.
- **Screen-space-error mode** (`ScreenSpaceError.h`): converts a
  `maxScreenSpaceErrorPx` budget and each LOD's `geometricError` into an
  equivalent distance threshold, then reuses the exact same distance
  selection function. This works because, for fixed geometric error, a
  screen-space error's pixel size is a simple inverse function of camera
  distance — so "the coarsest LOD whose error is still within budget" is
  the same shape of answer as "the coarsest LOD whose threshold distance
  the camera has passed." `screenSpaceErrorCrossoverDistance` computes that
  per-LOD threshold in closed form; no iteration or bisection needed.

This is why `LODManager` has one hysteresis implementation instead of two:
whichever mode is active, `selectLOD` ends up comparing the camera distance
against an ascending threshold list either way.

## Hysteresis

Config: `hysteresisRatio` (default `0.1`, a 10% band). Only applies to a
single-step adjacent LOD change — jumping more than one LOD (e.g. the
camera teleports) snaps immediately, since a dead zone that only exists to
suppress flicker at one boundary has no reason to slow down a legitimate
large change.

For an adjacent change from LOD `current` to `current + 1` (moving
coarser), the boundary distance is `thresholds[current]`; the change is
accepted only once the camera distance exceeds
`thresholds[current] * (1 + hysteresisRatio)`. Moving finer
(`current - 1`) is the mirror case: accepted only once distance drops below
`thresholds[current - 1] * (1 - hysteresisRatio)`. A camera sitting exactly
on the raw boundary therefore doesn't flip every frame — see
`tests/lod/LODManagerTests.cpp` for the exact numbers.

## Why generic over a key

`docs/architecture.md`'s dependency graph has `Core` depend on nothing, and
Streaming depend on `LOD` — not the reverse, and not on `Data`. Since
per-tile hysteresis needs *some* way to remember "what LOD was this tile at
last frame," `LODManager<Key>` is templated on an opaque, hashable key
(`spatial::data::TileId` in practice) instead of hard-coding a dependency
on the tile model. This mirrors the same fix applied to `SpatialIndex<T>`
in Phase 4.

## Why LOD doesn't drive streaming

The binary tile format (Phase 3) stores every LOD for a tile in one file —
loading a tile loads all of its detail levels at once. So unlike a system
where each LOD is a separately downloadable asset, LOD selection here never
triggers a load; it only decides which of an *already-resident* tile's LOD
meshes to use. That makes it purely a render-time (Phase 8) concern, safe
to test and reason about in complete isolation from streaming — which is
exactly what `tests/lod/` does.
