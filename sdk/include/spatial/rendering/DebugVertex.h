#pragma once

#include "spatial/core/Vec3.h"
#include "spatial/rendering/Color.h"

namespace spatial::rendering
{
    struct DebugVertex
    {
        core::Vec3 position;
        Color color;
    };
}
