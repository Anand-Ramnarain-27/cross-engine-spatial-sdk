#include "spatial/data/TileIndex.h"

#include <cmath>

namespace spatial::data
{
    namespace
    {
        [[nodiscard]] bool isPowerOfTwo(std::uint32_t v) noexcept { return v > 0 && (v & (v - 1)) == 0; }

        [[nodiscard]] std::uint32_t log2PowerOfTwo(std::uint32_t v) noexcept
        {
            std::uint32_t level = 0;
            while (v > 1)
            {
                v >>= 1;
                ++level;
            }
            return level;
        }
    }

    TileIndex::TileIndex(const core::AABB& worldBounds) : m_spatialIndex(worldBounds) {}

    void TileIndex::insert(TileId id, const core::AABB& bounds)
    {
        m_bounds[id] = bounds;
        m_spatialIndex.insert(id, bounds);
    }

    std::optional<core::AABB> TileIndex::find(const TileId& id) const
    {
        const auto it = m_bounds.find(id);
        if (it == m_bounds.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::size_t TileIndex::size() const noexcept { return m_bounds.size(); }

    std::vector<TileId> TileIndex::queryFrustum(const core::Frustum& frustum) const
    {
        return m_spatialIndex.queryFrustum(frustum);
    }

    std::vector<TileId> TileIndex::queryRadius(const core::Vec3& center, float radius) const
    {
        return m_spatialIndex.queryRadius(center, radius);
    }

    Expected<TileIndex> TileIndex::buildUniformGrid(const DatasetManifest& manifest)
    {
        if (manifest.tileSize <= 0.0f || manifest.worldSize <= 0.0f)
        {
            return Error{ErrorCode::InvalidDataset, "tileSize and worldSize must be positive"};
        }

        const float ratio = manifest.worldSize / manifest.tileSize;
        const auto gridSize = static_cast<std::uint32_t>(std::lround(ratio));
        if (gridSize == 0 || std::abs(ratio - static_cast<float>(gridSize)) > 0.01f)
        {
            return Error{ErrorCode::InvalidDataset, "worldSize is not an exact multiple of tileSize"};
        }
        if (!isPowerOfTwo(gridSize))
        {
            return Error{ErrorCode::InvalidDataset, "worldSize / tileSize must be a power of two"};
        }

        const std::uint32_t level = log2PowerOfTwo(gridSize);
        const core::AABB worldBounds = manifest.worldBounds();
        const float worldMinX = worldBounds.min.x;
        const float worldMinZ = worldBounds.min.z;

        TileIndex index(worldBounds);
        for (std::uint32_t y = 0; y < gridSize; ++y)
        {
            for (std::uint32_t x = 0; x < gridSize; ++x)
            {
                const float minX = worldMinX + static_cast<float>(x) * manifest.tileSize;
                const float minZ = worldMinZ + static_cast<float>(y) * manifest.tileSize;
                const core::AABB tileBounds{
                    core::Vec3{minX, worldBounds.min.y, minZ},
                    core::Vec3{minX + manifest.tileSize, worldBounds.max.y, minZ + manifest.tileSize},
                };
                index.insert(TileId{level, x, y}, tileBounds);
            }
        }

        return index;
    }
}
