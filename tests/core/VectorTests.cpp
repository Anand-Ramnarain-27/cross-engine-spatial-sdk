#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/Vec2.h"
#include "spatial/core/Vec3.h"
#include "spatial/core/Vec4.h"

using namespace spatial::core;
using Catch::Approx;

TEST_CASE("Vec2 arithmetic and normalization", "[core][math][vec2]")
{
    const Vec2 a{3.0f, 4.0f};

    CHECK(a.lengthSquared() == Approx(25.0f));
    CHECK(a.length() == Approx(5.0f));

    const Vec2 n = a.normalized();
    CHECK(n.length() == Approx(1.0f));

    CHECK((Vec2{1, 2} + Vec2{3, 4}) == Vec2{4, 6});
    CHECK((Vec2{1, 2} - Vec2{3, 4}) == Vec2{-2, -2});
    CHECK((Vec2{1, 2} * 2.0f) == Vec2{2, 4});
    CHECK(dot(Vec2{1, 0}, Vec2{0, 1}) == Approx(0.0f));

    CHECK(Vec2{}.normalized() == Vec2{});
}

TEST_CASE("Vec3 arithmetic, dot, and cross", "[core][math][vec3]")
{
    CHECK((Vec3{1, 2, 3} + Vec3{4, 5, 6}) == Vec3{5, 7, 9});
    CHECK((Vec3{4, 5, 6} - Vec3{1, 2, 3}) == Vec3{3, 3, 3});
    CHECK((Vec3{1, 2, 3} * 2.0f) == Vec3{2, 4, 6});

    CHECK(dot(Vec3::unitX(), Vec3::unitY()) == Approx(0.0f));
    CHECK(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}) == Approx(32.0f));

    // Right-handed basis: x cross y == z.
    CHECK(nearlyEqual(cross(Vec3::unitX(), Vec3::unitY()), Vec3::unitZ()));

    CHECK(Vec3{3, 0, 4}.length() == Approx(5.0f));
    CHECK(Vec3{}.normalized() == Vec3{});

    CHECK(distance(Vec3{0, 0, 0}, Vec3{3, 4, 0}) == Approx(5.0f));
}

TEST_CASE("Vec4 basics", "[core][math][vec4]")
{
    const Vec4 v{Vec3{1, 2, 3}, 1.0f};
    CHECK(v.xyz() == Vec3{1, 2, 3});
    CHECK(v.w == Approx(1.0f));

    CHECK(dot(Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}) == Approx(0.0f));
    CHECK((Vec4{1, 2, 3, 4} + Vec4{1, 1, 1, 1}) == Vec4{2, 3, 4, 5});
}
