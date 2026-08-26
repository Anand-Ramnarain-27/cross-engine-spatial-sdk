#pragma once

#include <optional>
#include <vector>

#include "spatial/core/AABB.h"
#include "spatial/data/Material.h"
#include "spatial/data/Metadata.h"
#include "spatial/data/TileId.h"
#include "spatial/data/TileLOD.h"

namespace spatial::data
{
    // A node in the tile quadtree. Plain data, loaded/saved by TileSerializer;
    // streaming/GPU residency state lives elsewhere (spatial::streaming).
    class Tile
    {
    public:
        Tile() = default;
        explicit Tile(TileId id) : m_id(id) {}

        [[nodiscard]] TileId id() const noexcept { return m_id; }
        void setId(TileId id) noexcept { m_id = id; }

        [[nodiscard]] const core::AABB& bounds() const noexcept { return m_bounds; }
        void setBounds(const core::AABB& bounds) noexcept { m_bounds = bounds; }

        [[nodiscard]] const std::optional<TileId>& parent() const noexcept { return m_parent; }
        void setParent(std::optional<TileId> parent) noexcept { m_parent = parent; }

        [[nodiscard]] const std::vector<TileId>& children() const noexcept { return m_children; }
        void setChildren(std::vector<TileId> children) { m_children = std::move(children); }
        void addChild(TileId child) { m_children.push_back(child); }

        [[nodiscard]] const std::vector<TileLOD>& lods() const noexcept { return m_lods; }
        void setLODs(std::vector<TileLOD> lods) { m_lods = std::move(lods); }
        void addLOD(TileLOD lod) { m_lods.push_back(std::move(lod)); }
        [[nodiscard]] std::size_t lodCount() const noexcept { return m_lods.size(); }

        [[nodiscard]] const std::vector<Material>& materials() const noexcept { return m_materials; }
        void setMaterials(std::vector<Material> materials) { m_materials = std::move(materials); }

        [[nodiscard]] const Metadata& metadata() const noexcept { return m_metadata; }
        [[nodiscard]] Metadata& metadata() noexcept { return m_metadata; }

    private:
        TileId m_id;
        core::AABB m_bounds;
        std::optional<TileId> m_parent;
        std::vector<TileId> m_children;
        std::vector<TileLOD> m_lods;
        std::vector<Material> m_materials;
        Metadata m_metadata;
    };
}
