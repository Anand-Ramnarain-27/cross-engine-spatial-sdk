#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/Plane.h"

using namespace spatial::core;
using Catch::Approx;

TEST_CASE("Plane::fromNormalAndPoint places the point on the plane", "[core][bounds][plane]")
{
    const Plane p = Plane::fromNormalAndPoint(Vec3{0, 1, 0}, Vec3{0, 5, 0});
    CHECK(p.signedDistance(Vec3{0, 5, 0}) == Approx(0.0f).margin(1e-5));
    CHECK(p.signedDistance(Vec3{100, 5, -100}) == Approx(0.0f).margin(1e-5));
}

TEST_CASE("Plane signedDistance sign matches the normal direction", "[core][bounds][plane]")
{
    const Plane p = Plane::fromNormalAndPoint(Vec3{0, 1, 0}, Vec3{0, 0, 0});
    CHECK(p.signedDistance(Vec3{0, 10, 0}) > 0.0f);
    CHECK(p.signedDistance(Vec3{0, -10, 0}) < 0.0f);
    CHECK(p.signedDistance(Vec3{0, 3, 0}) == Approx(3.0f));
}

TEST_CASE("Plane::normalized produces a unit normal and true distance", "[core][bounds][plane]")
{
    const Plane raw{Vec3{0, 2, 0}, 4.0f};
    const Plane n = raw.normalized();
    CHECK(n.normal.length() == Approx(1.0f));
    CHECK(n.signedDistance(Vec3{0, 0, 0}) == Approx(2.0f));
}
