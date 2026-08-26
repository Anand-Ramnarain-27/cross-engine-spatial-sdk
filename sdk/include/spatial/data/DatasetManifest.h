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

        core::CoordinateSystem coordinateSystem = core::CoordinateSystem::LocalCartesian;

        // Centered at the origin on X/Z; Y uses a generous fixed range since
        // dataset height isn't yet a manifest field.
        [[nodiscard]] core::AABB worldBounds() const noexcept
        {
            const float half = worldSize * 0.5f;
            return core::AABB{
                core::Vec3{-half, -1000.0f, -half},
                core::Vec3{half, 1000.0f, half},
            };
        }

        Metadata metadata;

        [[nodiscard]] bool operator==(const DatasetManifest&) const = default;
    };
}
