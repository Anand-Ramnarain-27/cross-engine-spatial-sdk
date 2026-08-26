#pragma once

#include <limits>

#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Axis-aligned bounding box. The primary bounding volume used for tile
    // bounds and spatial index nodes; Sphere is used where a tighter,
    // cheaper-to-test volume is worth the extra memory (see Sphere.h).
    struct AABB
    {
        Vec3 min{0.0f, 0.0f, 0.0f};
        Vec3 max{0.0f, 0.0f, 0.0f};

        constexpr AABB() = default;
        constexpr AABB(const Vec3& min_, const Vec3& max_) noexcept : min(min_), max(max_) {}

        [[nodiscard]] static constexpr AABB fromCenterExtents(const Vec3& center, const Vec3& extents) noexcept
        {
            return AABB{center - extents, center + extents};
        }

        // An AABB with no volume, positioned so that expanding it with any
        // point immediately produces a valid, correctly-sized box. Use this
        // as the starting accumulator when building a bounds from geometry.
        [[nodiscard]] static AABB empty() noexcept
        {
            constexpr float inf = std::numeric_limits<float>::infinity();
            return AABB{Vec3{inf, inf, inf}, Vec3{-inf, -inf, -inf}};
        }

        [[nodiscard]] constexpr Vec3 center() const noexcept { return (min + max) * 0.5f; }
        [[nodiscard]] constexpr Vec3 extents() const noexcept { return (max - min) * 0.5f; }
        [[nodiscard]] constexpr Vec3 size() const noexcept { return max - min; }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return min.x <= max.x && min.y <= max.y && min.z <= max.z;
        }

        [[nodiscard]] constexpr bool containsPoint(const Vec3& p) const noexcept
        {
            return p.x >= min.x && p.x <= max.x &&
                   p.y >= min.y && p.y <= max.y &&
                   p.z >= min.z && p.z <= max.z;
        }

        [[nodiscard]] constexpr bool intersectsAABB(const AABB& other) const noexcept
        {
            return min.x <= other.max.x && max.x >= other.min.x &&
                   min.y <= other.max.y && max.y >= other.min.y &&
                   min.z <= other.max.z && max.z >= other.min.z;
        }

        [[nodiscard]] constexpr AABB merged(const AABB& other) const noexcept
        {
            return AABB{minComponents(min, other.min), maxComponents(max, other.max)};
        }

        constexpr void expand(const Vec3& point) noexcept
        {
            min = minComponents(min, point);
            max = maxComponents(max, point);
        }

        // Closest-point distance; zero if p is inside the box.
        [[nodiscard]] constexpr float distanceSquaredToPoint(const Vec3& p) const noexcept
        {
            const float dx = p.x < min.x ? (min.x - p.x) : (p.x > max.x ? (p.x - max.x) : 0.0f);
            const float dy = p.y < min.y ? (min.y - p.y) : (p.y > max.y ? (p.y - max.y) : 0.0f);
            const float dz = p.z < min.z ? (min.z - p.z) : (p.z > max.z ? (p.z - max.z) : 0.0f);
            return dx * dx + dy * dy + dz * dz;
        }

        // The corner of the box furthest along `direction` — the "n-vertex"
        // used by the standard plane/AABB culling test (Frustum::intersectsAABB):
        // if this vertex is on the negative side of a plane, the whole box is.
        [[nodiscard]] constexpr Vec3 positiveVertex(const Vec3& direction) const noexcept
        {
            return Vec3{
                direction.x >= 0.0f ? max.x : min.x,
                direction.y >= 0.0f ? max.y : min.y,
                direction.z >= 0.0f ? max.z : min.z,
            };
        }

        [[nodiscard]] constexpr Vec3 negativeVertex(const Vec3& direction) const noexcept
        {
            return Vec3{
                direction.x >= 0.0f ? min.x : max.x,
                direction.y >= 0.0f ? min.y : max.y,
                direction.z >= 0.0f ? min.z : max.z,
            };
        }
    };
}
