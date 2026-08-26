#pragma once

#include <cstdint>
#include <string>

#include "spatial/core/AABB.h"
#include "spatial/core/CoordinateSystem.h"
#include "spatial/core/Vec3.h"
#include "spatial/data/Metadata.h"

namespace spatial::data
{
    inline constexpr std::uint32_t kDatasetManifestVersion = 1;

    // In-memory form of a dataset's ".world" manifest; see docs/tile_format.md.
    struct DatasetManifest
    {
        std::uint32_t version = kDatasetManifestVersion;
        std::string name;

        float tileSize = 100.0f;
        float worldSize = 10000.0f;
        std::uint32_t maxLOD = 0;

        // Optional in the JSON manifest; these defaults are the same generic
        // range used before per-dataset height bounds existed, so an older
        // manifest missing the fields behaves exactly as it did before.
        float worldHeightMin = -1000.0f;
        float worldHeightMax = 1000.0f;

        core::CoordinateSystem coordinateSystem = core::CoordinateSystem::LocalCartesian;

        // Centered at the origin on X/Z.
        [[nodiscard]] core::AABB worldBounds() const noexcept
        {
            const float half = worldSize * 0.5f;
            return core::AABB{
                core::Vec3{-half, worldHeightMin, -half},
                core::Vec3{half, worldHeightMax, half},
            };
        }

        Metadata metadata;

        [[nodiscard]] bool operator==(const DatasetManifest&) const = default;
    };
}
