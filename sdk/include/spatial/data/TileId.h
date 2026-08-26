#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace spatial::data
{
    // Quadtree address: level 0 is the root, level L has up to 2^L x 2^L tiles.
    struct TileId
    {
        std::uint32_t level = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;

        [[nodiscard]] constexpr bool operator==(const TileId&) const = default;

        [[nodiscard]] constexpr TileId parent() const noexcept
        {
            return TileId{level > 0 ? level - 1 : 0, x / 2, y / 2};
        }

        [[nodiscard]] constexpr TileId child(std::uint32_t dx, std::uint32_t dy) const noexcept
        {
            return TileId{level + 1, x * 2 + dx, y * 2 + dy};
        }

        [[nodiscard]] std::string toString() const
        {
            return "L" + std::to_string(level) + "_" + std::to_string(x) + "_" + std::to_string(y);
        }
    };
}

template <>
struct std::hash<spatial::data::TileId>
{
    [[nodiscard]] std::size_t operator()(const spatial::data::TileId& id) const noexcept
    {
        std::size_t seed = std::hash<std::uint32_t>{}(id.level);
        seed ^= std::hash<std::uint32_t>{}(id.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<std::uint32_t>{}(id.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
