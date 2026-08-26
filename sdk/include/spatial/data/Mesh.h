#pragma once

#include <cstdint>
#include <vector>

#include "spatial/core/Vec2.h"
#include "spatial/core/Vec3.h"

namespace spatial::data
{
    struct Vertex
    {
        core::Vec3 position;
        core::Vec3 normal;
        core::Vec2 uv;

        [[nodiscard]] bool operator==(const Vertex&) const = default;
    };

    // materialIndex indexes into the owning Tile's material list (-1 = none).
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::int32_t materialIndex = -1;

        [[nodiscard]] std::size_t triangleCount() const noexcept { return indices.size() / 3; }

        [[nodiscard]] bool operator==(const Mesh&) const = default;
    };
}
