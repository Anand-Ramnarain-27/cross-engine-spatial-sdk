#pragma once

#include <cstdint>
#include <vector>

namespace spatial::lod
{
    // thresholds[i] is the distance at which selection moves from LOD i to
    // LOD i+1. thresholds must be ascending. Returns an index in
    // [0, thresholds.size()] — thresholds.size() itself means "coarser than
    // every configured threshold".
    [[nodiscard]] inline std::uint32_t selectLODByDistance(float distance, const std::vector<float>& thresholds) noexcept
    {
        std::uint32_t lod = 0;
        for (const float threshold : thresholds)
        {
            if (distance < threshold)
            {
                return lod;
            }
            ++lod;
        }
        return lod;
    }
}
