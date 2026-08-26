#pragma once

#include <cstddef>
#include <cstdint>

#include "spatial/data/Tile.h"

namespace spatial::data
{
    // Rough estimate of a Tile's CPU memory footprint in bytes: vertex/index
    // buffers dominate, plus a coarse accounting for materials and metadata
    // strings. Used by TileCache to enforce a memory budget — doesn't need
    // to be exact (allocator overhead, alignment, etc. are ignored).
    [[nodiscard]] inline std::size_t estimateTileMemoryBytes(const Tile& tile) noexcept
    {
        std::size_t bytes = sizeof(Tile);
        for (const TileLOD& lod : tile.lods())
        {
            bytes += sizeof(TileLOD);
            for (const Mesh& mesh : lod.meshes)
            {
                bytes += mesh.vertices.size() * sizeof(Vertex);
                bytes += mesh.indices.size() * sizeof(std::uint32_t);
            }
        }
        bytes += tile.materials().size() * sizeof(Material);
        for (const auto& [key, value] : tile.metadata().entries())
        {
            bytes += key.size() + value.size();
        }
        return bytes;
    }
}
