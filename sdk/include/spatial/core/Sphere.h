#pragma once

#include "spatial/core/AABB.h"
#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Bounding sphere. Cheaper than an AABB to test against a plane or
    // another sphere (no per-axis branching), at the cost of a looser fit
    // for non-cubic geometry.
    struct Sphere
    {
        Vec3 center{0.0f, 0.0f, 0.0f};
        float radius = 0.0f;

        constexpr Sphere() = default;
        constexpr Sphere(const Vec3& center_, float radius_) noexcept : center(center_), radius(radius_) {}

        [[nodiscard]] static Sphere fromAABB(const AABB& box) noexcept
        {
            return Sphere{box.center(), box.extents().length()};
        }

        [[nodiscard]] bool containsPoint(const Vec3& p) const noexcept
        {
            return distanceSquared(center, p) <= radius * radius;
        }

        [[nodiscard]] bool intersectsSphere(const Sphere& other) const noexcept
        {
            const float r = radius + other.radius;
            return distanceSquared(center, other.center) <= r * r;
        }

        [[nodiscard]] constexpr bool intersectsAABB(const AABB& box) const noexcept
        {
            return box.distanceSquaredToPoint(center) <= radius * radius;
        }
    };
}
