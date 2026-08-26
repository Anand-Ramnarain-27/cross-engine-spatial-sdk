# Tile Format

Two on-disk formats: a JSON **dataset manifest** (one per dataset) and a
binary **tile format** (one file per tile). Both are versioned and reject
unrecognized/future versions explicitly (`ErrorCode::UnsupportedVersion`)
rather than guessing.

## Dataset manifest (`.world`, JSON)

```json
{
    "version": 1,
    "name": "ExampleCity",
    "tileSize": 100.0,
    "worldSize": 10000.0,
    "maxLOD": 4,
    "coordinateSystem": "LOCAL_CARTESIAN",
    "metadata": {
        "author": "SpatialTileBuilder"
    }
}
```

| Field | Type | Required | Meaning |
|---|---|---|---|
| `version` | uint | yes | Manifest format version. Currently `1`. |
| `name` | string | yes | Dataset name. |
| `tileSize` | number | yes | World-space footprint (X/Z) of one generated tile; see note below. |
| `worldSize` | number | yes | Total world width/depth; world bounds are `[-worldSize/2, worldSize/2]` on X and Z (see `DatasetManifest::worldBounds()`), with a fixed generous Y range since per-dataset height bounds aren't a manifest field yet. |
| `maxLOD` | uint | yes | Highest LOD level tiles may provide (0 = coarsest only). |
| `coordinateSystem` | string | yes | One of the values in `spatial::core::CoordinateSystem` (currently only `"LOCAL_CARTESIAN"`). |
| `metadata` | object | no | Free-form string-to-string key/value pairs. Non-string values are silently skipped. |

**Forward compatibility:** unrecognized top-level keys are ignored by the
loader, so new optional fields can be added without bumping `version`.
Loading fails with:
- `DatasetNotFound` — the file doesn't exist / can't be opened.
- `InvalidDataset` — malformed JSON, a required field is missing, or a field
  has the wrong type.
- `UnsupportedVersion` — `version` is `0` or greater than the version this
  SDK build understands (`spatial::data::kDatasetManifestVersion`).

`tileSize` note: `gridSize = worldSize / tileSize` must be a power of two;
`SpatialTileBuilder` generates one flat grid of `gridSize x gridSize` tiles
at `TileId.level = log2(gridSize)`. Each of those tiles independently has
`maxLOD + 1` levels of detail (`TileLOD`, see below) — LOD is a per-tile
geometric-detail axis, unrelated to `TileId.level`.

## Tile hierarchy addressing

Tiles are addressed by `TileId{level, x, y}` — the standard quadtree
"slippy map" scheme. Level 0 is the single root tile covering the whole
dataset; at level `L` there are up to `2^L x 2^L` tiles, each one quarter
the area of its parent. A tile's four children are always
`(level+1, 2x, 2y)`, `(2x+1, 2y)`, `(2x, 2y+1)`, `(2x+1, 2y+1)` — so parent
and child addresses are computed (`TileId::parent()`/`TileId::child()`), not
looked up. `SpatialTileBuilder` currently only populates one level; shallower
or deeper levels are addressable but not yet generated.

On disk, a tile's file is named `L{level}_{x}_{y}.tile`, e.g. `L1_0_1.tile`.

## Binary tile format (`.tile`)

All integers and floats are little-endian, fixed-width, densely packed (no
padding). This matches x86-64, the SDK's first supported platform; a
non-x86 port would add byte-swapping in one place (`sdk/src/data/BinaryIO.h`)
without touching the format layout below.

```
Header
  magic            char[4]     "SPTL"
  formatVersion    uint32      currently 1
  id               TileId      (see below)
  hasParent        uint8       0 or 1
  parent           TileId      present only if hasParent == 1
  childCount       uint32
  children         TileId[childCount]
  boundsMin        float32[3]
  boundsMax        float32[3]

LODs
  lodCount         uint32
  lods[lodCount]:
    geometricError   float32   world-space error this LOD represents
    meshCount        uint32    one mesh per material used at this LOD
    meshes[meshCount]:
      materialIndex    int32     index into Materials below, -1 = none
      vertexCount      uint32
      indexCount       uint32
      vertices[vertexCount]:
        position         float32[3]
        normal           float32[3]
        uv               float32[2]
      indices[indexCount]  uint32          (triangle list)

Materials
  materialCount    uint32
  materials[materialCount]:
    name             string    (see below)
    baseColorRGBA    float32[4]
    metallic         float32
    roughness        float32

Metadata
  entryCount       uint32
  entries[entryCount]:
    key              string
    value            string
```

`TileId` = `{ level: uint32, x: uint32, y: uint32 }`.

`string` = `{ length: uint32, bytes: uint8[length] }`, UTF-8, not
null-terminated.

**Corruption handling:** every length/count field is checked against a
sanity cap before it's used to size a container (`BinaryReader::readCount` /
`readString`, capped at 20,000,000 elements / 1 MiB respectively) — a
corrupted 32-bit count can't trigger a multi-gigabyte allocation attempt.
Loading fails with:
- `TileLoadFailed` — the file doesn't exist / can't be opened, or a write
  failed.
- `CorruptTile` — bad magic, a sanity cap was exceeded, or the file is
  truncated (an unexpected EOF is caught by checking the stream's fail
  state once at the end of the read, not after every field).
- `UnsupportedVersion` — `formatVersion` is `0` or greater than
  `spatial::data::kTileFormatVersion`.

## Known simplifications (Phase 3 scope)

- Materials reference no textures yet — `Material` is color/metallic/
  roughness only. Texture references are added when `TextureResource` is
  introduced in Phase 8 (Rendering).
- World height (Y) bounds are not yet a manifest field; see the `worldSize`
  row above.
