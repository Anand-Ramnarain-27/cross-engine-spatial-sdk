#pragma once

#include <numbers>

#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Minimal per-frame camera snapshot shared by LOD selection and
    // streaming (both only need position/orientation/projection, not a
    // full Camera object with view/projection matrices).
    struct CameraParams
    {
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 forward{0.0f, 0.0f, -1.0f}; // normalized

        float verticalFovRadians = std::numbers::pi_v<float> / 3.0f; // 60 degrees
        float viewportHeightPx = 1080.0f;
    };
}
