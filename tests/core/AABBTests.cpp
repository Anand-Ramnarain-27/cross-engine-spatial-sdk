#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"

using namespace spatial::core;
using Catch::Approx;

TEST_CASE("AABB center, extents, and size", "[core][bounds][aabb]")
{
    const AABB box{Vec3{-1, -2, -3}, Vec3{1, 2, 3}};
    CHECK(nearlyEqual(box.center(), Vec3{0, 0, 0}));
    CHECK(nearlyEqual(box.extents(), Vec3{1, 2, 3}));
    CHECK(nearlyEqual(box.size(), Vec3{2, 4, 6}));
    CHECK(box.isValid());
}

TEST_CASE("AABB::fromCenterExtents round-trips", "[core][bounds][aabb]")
{
    const AABB box = AABB::fromCenterExtents(Vec3{10, 0, 0}, Vec3{5, 5, 5});
    CHECK(nearlyEqual(box.min, Vec3{5, -5, -5}));
    CHECK(nearlyEqual(box.max, Vec3{15, 5, 5}));
}

TEST_CASE("AABB containsPoint respects boundaries inclusively", "[core][bounds][aabb]")
{
    const AABB box{Vec3{0, 0, 0}, Vec3{10, 10, 10}};
    CHECK(box.containsPoint(Vec3{5, 5, 5}));
    CHECK(box.containsPoint(Vec3{0, 0, 0}));
    CHECK(box.containsPoint(Vec3{10, 10, 10}));
    CHECK_FALSE(box.containsPoint(Vec3{10.001f, 5, 5}));
}

TEST_CASE("AABB intersectsAABB", "[core][bounds][aabb]")
{
    const AABB a{Vec3{0, 0, 0}, Vec3{10, 10, 10}};
    const AABB overlapping{Vec3{5, 5, 5}, Vec3{15, 15, 15}};
    const AABB touching{Vec3{10, 0, 0}, Vec3{20, 10, 10}};
    const AABB separate{Vec3{20, 20, 20}, Vec3{30, 30, 30}};

    CHECK(a.intersectsAABB(overlapping));
    CHECK(a.intersectsAABB(touching));
    CHECK_FALSE(a.intersectsAABB(separate));
}

TEST_CASE("AABB expand and merged grow the bounds", "[core][bounds][aabb]")
{
    AABB box = AABB::empty();
    box.expand(Vec3{1, 2, 3});
    box.expand(Vec3{-1, 5, 0});
    CHECK(nearlyEqual(box.min, Vec3{-1, 2, 0}));
    CHECK(nearlyEqual(box.max, Vec3{1, 5, 3}));

    const AABB other{Vec3{-10, -10, -10}, Vec3{-5, -5, -5}};
    const AABB merged = box.merged(other);
    CHECK(nearlyEqual(merged.min, Vec3{-10, -10, -10}));
    CHECK(nearlyEqual(merged.max, Vec3{1, 5, 3}));
}

TEST_CASE("AABB distanceSquaredToPoint is zero inside, positive outside", "[core][bounds][aabb]")
{
    const AABB box{Vec3{0, 0, 0}, Vec3{10, 10, 10}};
    CHECK(box.distanceSquaredToPoint(Vec3{5, 5, 5}) == Approx(0.0f));
    CHECK(box.distanceSquaredToPoint(Vec3{15, 0, 0}) == Approx(25.0f));
    CHECK(box.distanceSquaredToPoint(Vec3{13, 14, 0}) == Approx(9.0f + 16.0f));
}

TEST_CASE("AABB positiveVertex/negativeVertex pick the correct corner", "[core][bounds][aabb]")
{
    const AABB box{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    CHECK(nearlyEqual(box.positiveVertex(Vec3{1, 1, 1}), Vec3{1, 1, 1}));
    CHECK(nearlyEqual(box.negativeVertex(Vec3{1, 1, 1}), Vec3{-1, -1, -1}));
    CHECK(nearlyEqual(box.positiveVertex(Vec3{-1, 1, -1}), Vec3{-1, 1, -1}));
}
