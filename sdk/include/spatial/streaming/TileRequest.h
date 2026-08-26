#pragma once

#include "spatial/data/TileId.h"

namespace spatial::streaming
{
    struct TileRequest
    {
        data::TileId id;
        float priority = 0.0f;
    };

    // Higher priority sorts first in a std::priority_queue (a max-heap).
    [[nodiscard]] constexpr bool operator<(const TileRequest& lhs, const TileRequest& rhs) noexcept
    {
        return lhs.priority < rhs.priority;
    }
}
