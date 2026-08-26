#pragma once

#include <vector>

#include "spatial/data/Mesh.h"

namespace spatial::data
{
    // geometricError is the world-space deviation from full-detail geometry.
    // One Mesh per material used at this LOD (e.g. ground vs. buildings).
    struct TileLOD
    {
        float geometricError = 0.0f;
        std::vector<Mesh> meshes;

        [[nodiscard]] bool operator==(const TileLOD&) const = default;
    };
}
