#pragma once

#include <cstdint>

#include "spatial/core/AABB.h"
#include "spatial/data/Tile.h"
#include "spatial/data/TileId.h"

namespace spatial::tools
{
    struct ProceduralCityConfig
    {
        std::uint32_t buildingsPerTileSide = 3;      // an N x N grid of buildings per tile
        float buildingFootprintFraction = 0.6f;      // fraction of each grid cell the building occupies
        float minBuildingHeight = 5.0f;
        float maxBuildingHeight = 60.0f;
        std::uint32_t seed = 1;                      // combined with tile (x, y) for reproducible layout
    };

    // Generates LODs 0..maxLOD for the tile at `id`; each LOD keeps half as
    // many buildings as the last, evenly spread across the tile.
    [[nodiscard]] data::Tile generateProceduralTile(
        const data::TileId& id,
        const core::AABB& bounds,
        const ProceduralCityConfig& config,
        std::uint32_t maxLOD);
}
