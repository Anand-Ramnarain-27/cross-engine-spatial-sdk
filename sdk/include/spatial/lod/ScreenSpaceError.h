#pragma once

#include <cmath>
#include <limits>

namespace spatial::lod
{
    // Projects a world-space geometric error to a screen-space pixel error,
    // given a camera distance and vertical FOV/viewport. Standard formula
    // used by e.g. Cesium 3D Tiles: error scales inversely with distance and
    // directly with how many pixels the viewport's vertical FOV spans.
    [[nodiscard]] inline float computeScreenSpaceError(
        float geometricError,
        float distance,
        float verticalFovRadians,
        float viewportHeightPx) noexcept
    {
        if (distance <= 0.0f)
        {
            return geometricError > 0.0f ? std::numeric_limits<float>::infinity() : 0.0f;
        }
        const float k = viewportHeightPx / (2.0f * std::tan(verticalFovRadians * 0.5f));
        return (geometricError * k) / distance;
    }

    // Inverse of computeScreenSpaceError: the distance at which a given
    // geometricError's screen-space error exactly equals maxErrorPx. Used to
    // turn a screen-space-error budget into per-LOD distance thresholds so
    // LOD selection can share one distance-threshold implementation
    // regardless of which metric picked the thresholds.
    [[nodiscard]] inline float screenSpaceErrorCrossoverDistance(
        float geometricError,
        float verticalFovRadians,
        float viewportHeightPx,
        float maxErrorPx) noexcept
    {
        if (geometricError <= 0.0f || maxErrorPx <= 0.0f)
        {
            return 0.0f;
        }
        const float k = viewportHeightPx / (2.0f * std::tan(verticalFovRadians * 0.5f));
        return (geometricError * k) / maxErrorPx;
    }
}
