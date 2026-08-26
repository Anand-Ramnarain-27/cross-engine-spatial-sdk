#pragma once

#include "spatial/core/CameraParams.h"
#include "spatial/core/Mat4.h"
#include "spatial/core/Vec3.h"

namespace viewer
{
    // Free-fly (noclip) camera: WASD + mouse look, no physics/collision.
    // yaw/pitch -> forward direction, matching spatial::core::Mat4's
    // convention (forward = (0,0,-1) at yaw=0, pitch=0).
    class FlyCamera
    {
    public:
        spatial::core::Vec3 position{0.0f, 80.0f, 260.0f};
        float yawRadians = 0.0f;
        float pitchRadians = -0.25f;

        float moveSpeed = 80.0f; // units/sec
        float mouseSensitivity = 0.0025f;

        // dx, dy: raw mouse movement in pixels since the last call.
        void look(float dx, float dy) noexcept;

        // localDirection: x=right, y=up, z=forward, each in [-1, 1]. Scaled
        // by moveSpeed * dt internally.
        void move(const spatial::core::Vec3& localDirection, float dt) noexcept;

        [[nodiscard]] spatial::core::Vec3 forward() const noexcept;
        [[nodiscard]] spatial::core::Vec3 right() const noexcept;

        [[nodiscard]] spatial::core::Mat4 viewMatrix() const noexcept;
        [[nodiscard]] spatial::core::CameraParams toCameraParams(float viewportHeightPx, float verticalFovRadians) const noexcept;
    };
}
