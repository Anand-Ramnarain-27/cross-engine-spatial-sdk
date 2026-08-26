#pragma once

#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Plane in normal-distance form: normal . p + distance = 0.
    struct Plane
    {
        Vec3 normal{0.0f, 1.0f, 0.0f};
        float distance = 0.0f;

        constexpr Plane() = default;
        constexpr Plane(const Vec3& normal_, float distance_) noexcept : normal(normal_), distance(distance_) {}

        [[nodiscard]] static Plane fromNormalAndPoint(const Vec3& normal, const Vec3& point) noexcept
        {
            const Vec3 n = normal.normalized();
            return Plane{n, -dot(n, point)};
        }

        [[nodiscard]] constexpr float signedDistance(const Vec3& p) const noexcept
        {
            return dot(normal, p) + distance;
        }

        [[nodiscard]] Plane normalized() const noexcept
        {
            const float len = normal.length();
            if (len <= 0.0f)
            {
                return *this;
            }
            return Plane{normal / len, distance / len};
        }
    };
}
