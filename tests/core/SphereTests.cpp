#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/Sphere.h"

using namespace spatial::core;
using Catch::Approx;

TEST_CASE("Sphere containsPoint", "[core][bounds][sphere]")
{
    const Sphere s{Vec3{0, 0, 0}, 5.0f};
    CHECK(s.containsPoint(Vec3{0, 0, 0}));
    CHECK(s.containsPoint(Vec3{5, 0, 0}));
    CHECK_FALSE(s.containsPoint(Vec3{5.001f, 0, 0}));
}

TEST_CASE("Sphere intersectsSphere", "[core][bounds][sphere]")
{
    const Sphere a{Vec3{0, 0, 0}, 5.0f};
    const Sphere overlapping{Vec3{8, 0, 0}, 5.0f};
    const Sphere separate{Vec3{20, 0, 0}, 5.0f};

    CHECK(a.intersectsSphere(overlapping));
    CHECK_FALSE(a.intersectsSphere(separate));
}

TEST_CASE("Sphere intersectsAABB", "[core][bounds][sphere]")
{
    const AABB box{Vec3{0, 0, 0}, Vec3{10, 10, 10}};
    const Sphere inside{Vec3{5, 5, 5}, 1.0f};
    const Sphere touching{Vec3{15, 5, 5}, 5.0f};
    const Sphere separate{Vec3{50, 50, 50}, 1.0f};

    CHECK(inside.intersectsAABB(box));
    CHECK(touching.intersectsAABB(box));
    CHECK_FALSE(separate.intersectsAABB(box));
}

TEST_CASE("Sphere::fromAABB bounds the box corners", "[core][bounds][sphere]")
{
    const AABB box{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    const Sphere s = Sphere::fromAABB(box);
    CHECK(nearlyEqual(s.center, Vec3{0, 0, 0}));
    // Radius must reach the box's corners (half-diagonal), not just its faces.
    CHECK(s.radius == Approx(Vec3{1, 1, 1}.length()));
}
