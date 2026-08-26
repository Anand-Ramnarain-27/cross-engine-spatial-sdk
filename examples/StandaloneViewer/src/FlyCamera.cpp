#include "FlyCamera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace spatial::core;

namespace viewer
{
    namespace
    {
        constexpr float kMaxPitch = std::numbers::pi_v<float> / 2.0f - 0.01f;
    }

    void FlyCamera::look(float dx, float dy) noexcept
    {
        yawRadians += dx * mouseSensitivity;
        pitchRadians = std::clamp(pitchRadians - dy * mouseSensitivity, -kMaxPitch, kMaxPitch);
    }

    Vec3 FlyCamera::forward() const noexcept
    {
        return Vec3{
            std::cos(pitchRadians) * std::sin(yawRadians),
            std::sin(pitchRadians),
            -std::cos(pitchRadians) * std::cos(yawRadians),
        };
    }

    Vec3 FlyCamera::right() const noexcept { return cross(forward(), Vec3::unitY()).normalized(); }

    void FlyCamera::move(const Vec3& localDirection, float dt) noexcept
    {
        const Vec3 f = forward();
        const Vec3 r = right();
        position += (r * localDirection.x + Vec3::unitY() * localDirection.y + f * localDirection.z) * moveSpeed * dt;
    }

    Mat4 FlyCamera::viewMatrix() const noexcept { return Mat4::lookAt(position, position + forward(), Vec3::unitY()); }

    CameraParams FlyCamera::toCameraParams(float viewportHeightPx, float verticalFovRadians) const noexcept
    {
        CameraParams params{};
        params.position = position;
        params.forward = forward();
        params.verticalFovRadians = verticalFovRadians;
        params.viewportHeightPx = viewportHeightPx;
        return params;
    }
}
