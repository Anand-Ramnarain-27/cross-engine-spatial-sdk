#pragma once

#include <array>
#include <cstddef>

#include "spatial/core/AABB.h"
#include "spatial/core/Mat4.h"
#include "spatial/core/Plane.h"
#include "spatial/core/Sphere.h"
#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Camera view frustum as six inward-facing planes. Built from a combined
    // view-projection matrix using the standard Gribb/Hartmann extraction:
    // each plane's coefficients are a row of the projection matrix added to
    // or subtracted from the last row, which is possible because clip-space
    // membership (-w <= x,y,z <= w) is linear in the rows of the combined
    // matrix. See Mat4.h for the matrix convention this relies on.
    struct Frustum
    {
        enum class PlaneIndex : std::size_t
        {
            Left = 0,
            Right = 1,
            Bottom = 2,
            Top = 3,
            Near = 4,
            Far = 5,
            Count = 6,
        };

        std::array<Plane, 6> planes{};

        [[nodiscard]] static Frustum fromViewProjection(const Mat4& viewProjection) noexcept
        {
            const Mat4& vp = viewProjection;
            auto rowSum = [&](int a, int sign, int b) {
                return Plane{
                    Vec3{
                        vp(a, 0) + static_cast<float>(sign) * vp(b, 0),
                        vp(a, 1) + static_cast<float>(sign) * vp(b, 1),
                        vp(a, 2) + static_cast<float>(sign) * vp(b, 2),
                    },
                    vp(a, 3) + static_cast<float>(sign) * vp(b, 3),
                }.normalized();
            };

            Frustum frustum{};
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Left)] = rowSum(3, +1, 0);
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Right)] = rowSum(3, -1, 0);
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Bottom)] = rowSum(3, +1, 1);
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Top)] = rowSum(3, -1, 1);
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Near)] = rowSum(3, +1, 2);
            frustum.planes[static_cast<std::size_t>(PlaneIndex::Far)] = rowSum(3, -1, 2);
            return frustum;
        }

        [[nodiscard]] bool containsPoint(const Vec3& p) const noexcept
        {
            for (const Plane& plane : planes)
            {
                if (plane.signedDistance(p) < 0.0f)
                {
                    return false;
                }
            }
            return true;
        }

        // Conservative test: returns false only when the box is fully
        // outside at least one plane. A box that merely straddles a plane
        // (partially visible) is reported as intersecting, which is the
        // correct, safe behavior for visibility culling.
        [[nodiscard]] bool intersectsAABB(const AABB& box) const noexcept
        {
            for (const Plane& plane : planes)
            {
                const Vec3 positive = box.positiveVertex(plane.normal);
                if (plane.signedDistance(positive) < 0.0f)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool intersectsSphere(const Sphere& sphere) const noexcept
        {
            for (const Plane& plane : planes)
            {
                if (plane.signedDistance(sphere.center) < -sphere.radius)
                {
                    return false;
                }
            }
            return true;
        }
    };
}
