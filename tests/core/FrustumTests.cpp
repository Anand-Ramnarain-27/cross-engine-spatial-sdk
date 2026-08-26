#include <numbers>

#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"
#include "spatial/core/Frustum.h"
#include "spatial/core/Mat4.h"
#include "spatial/core/Sphere.h"

using namespace spatial::core;

namespace
{
    // 90-degree vertical FOV puts the side planes at 45 degrees, so
    // in/out points at a given depth are easy to compute by hand.
    Mat4 testViewProjection()
    {
        const Mat4 view = Mat4::identity();
        const Mat4 proj = Mat4::perspective(std::numbers::pi_v<float> / 2.0f, 1.0f, 1.0f, 100.0f);
        return proj * view;
    }
}

TEST_CASE("Frustum containsPoint classifies near/far/side cases", "[core][culling][frustum]")
{
    const Frustum frustum = Frustum::fromViewProjection(testViewProjection());

    CHECK(frustum.containsPoint(Vec3{0, 0, -10}));      // centered, well within near/far
    CHECK_FALSE(frustum.containsPoint(Vec3{0, 0, 10}));  // behind the camera
    CHECK_FALSE(frustum.containsPoint(Vec3{0, 0, -0.5f})); // closer than the near plane
    CHECK_FALSE(frustum.containsPoint(Vec3{0, 0, -200}));  // beyond the far plane

    // At z = -10 the 90-degree FOV half-width is 10, so x = +-9 is inside
    // and x = +-11 is outside.
    CHECK(frustum.containsPoint(Vec3{9, 0, -10}));
    CHECK_FALSE(frustum.containsPoint(Vec3{11, 0, -10}));
    CHECK(frustum.containsPoint(Vec3{0, 9, -10}));
    CHECK_FALSE(frustum.containsPoint(Vec3{0, 11, -10}));
}

TEST_CASE("Frustum intersectsAABB culls boxes fully outside a plane", "[core][culling][frustum]")
{
    const Frustum frustum = Frustum::fromViewProjection(testViewProjection());

    const AABB inView{Vec3{-1, -1, -11}, Vec3{1, 1, -9}};
    CHECK(frustum.intersectsAABB(inView));

    const AABB straddling{Vec3{-1, -1, -1.5f}, Vec3{1, 1, 5}};
    CHECK(frustum.intersectsAABB(straddling)); // straddles the near plane: still visible

    const AABB behindCamera{Vec3{-1, -1, 1}, Vec3{1, 1, 5}};
    CHECK_FALSE(frustum.intersectsAABB(behindCamera));

    const AABB farAway{Vec3{500, 500, -10}, Vec3{510, 510, -9}};
    CHECK_FALSE(frustum.intersectsAABB(farAway));
}

TEST_CASE("Frustum intersectsSphere accounts for radius", "[core][culling][frustum]")
{
    const Frustum frustum = Frustum::fromViewProjection(testViewProjection());

    CHECK(frustum.intersectsSphere(Sphere{Vec3{0, 0, -10}, 1.0f}));

    // Center sits in the gap before the near plane (z = -1).
    CHECK_FALSE(frustum.intersectsSphere(Sphere{Vec3{0, 0, 0.5f}, 0.4f}));
    CHECK(frustum.intersectsSphere(Sphere{Vec3{0, 0, 0.5f}, 2.0f}));

    CHECK_FALSE(frustum.intersectsSphere(Sphere{Vec3{0, 0, 50}, 1.0f}));
}
