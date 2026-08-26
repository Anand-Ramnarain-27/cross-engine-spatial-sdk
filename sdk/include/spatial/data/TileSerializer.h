#pragma once

#include <filesystem>

#include "spatial/Error.h"
#include "spatial/Export.h"
#include "spatial/data/Tile.h"

namespace spatial::data
{
    // See docs/tile_format.md for the full byte layout.
    inline constexpr std::uint32_t kTileFormatVersion = 1;

    namespace TileSerializer
    {
        [[nodiscard]] SPATIAL_API Expected<Tile> loadTile(const std::filesystem::path& path);
        [[nodiscard]] SPATIAL_API Expected<void> saveTile(const Tile& tile, const std::filesystem::path& path);
    }
}
