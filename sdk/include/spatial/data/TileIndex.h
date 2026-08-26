#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include "spatial/Error.h"
#include "spatial/Export.h"
#include "spatial/core/AABB.h"
#include "spatial/core/Frustum.h"
#include "spatial/core/SpatialIndex.h"
#include "spatial/core/Vec3.h"
#include "spatial/data/DatasetManifest.h"
#include "spatial/data/TileId.h"

namespace spatial::data
{
    // Indexes which tiles exist and where, not their content — bounds only,
    // no geometry. Tile content is loaded on demand elsewhere (streaming).
    class SPATIAL_API TileIndex
    {
    public:
        explicit TileIndex(const core::AABB& worldBounds);

        void insert(TileId id, const core::AABB& bounds);

        [[nodiscard]] std::optional<core::AABB> find(const TileId& id) const;
        [[nodiscard]] std::size_t size() const noexcept;

        [[nodiscard]] std::vector<TileId> queryFrustum(const core::Frustum& frustum) const;
        [[nodiscard]] std::vector<TileId> queryRadius(const core::Vec3& center, float radius) const;

        // Builds an index covering every tile SpatialTileBuilder would
        // generate for `manifest`, computing bounds from tileSize/worldSize
        // alone — no tile files are read.
        [[nodiscard]] static Expected<TileIndex> buildUniformGrid(const DatasetManifest& manifest);

    private:
        core::SpatialIndex<TileId> m_spatialIndex;
        std::unordered_map<TileId, core::AABB> m_bounds;
    };
}
