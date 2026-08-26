#pragma once

#include <filesystem>

#include "spatial/Error.h"
#include "spatial/Export.h"
#include "spatial/data/DatasetManifest.h"

namespace spatial::data
{
    // Reads/writes the dataset manifest (".world" file, JSON). See docs/tile_format.md.
    namespace DatasetSerializer
    {
        [[nodiscard]] SPATIAL_API Expected<DatasetManifest> loadManifest(const std::filesystem::path& path);
        [[nodiscard]] SPATIAL_API Expected<void> saveManifest(const DatasetManifest& manifest, const std::filesystem::path& path);
    }
}
