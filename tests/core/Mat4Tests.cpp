#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/Mat4.h"
#include "spatial/core/Vec3.h"

using namespace spatial::core;
using Catch::Approx;

TEST_CASE("Mat4 identity leaves points unchanged", "[core][math][mat4]")
{
    const Vec3 p{1.0f, 2.0f, 3.0f};
    CHECK(nearlyEqual(Mat4::identity().transformPoint(p), p));
}

TEST_CASE("Mat4 translation and scale", "[core][math][mat4]")
{
    const Mat4 t = Mat4::translation(Vec3{10.0f, 0.0f, 0.0f});
    CHECK(nearlyEqual(t.transformPoint(Vec3{0, 0, 0}), Vec3{10, 0, 0}));
    // Translation must not affect directions.
    CHECK(nearlyEqual(t.transformVector(Vec3{1, 0, 0}), Vec3{1, 0, 0}));

    const Mat4 s = Mat4::scale(Vec3{2.0f, 3.0f, 4.0f});
    CHECK(nearlyEqual(s.transformPoint(Vec3{1, 1, 1}), Vec3{2, 3, 4}));
}

TEST_CASE("Mat4 composition applies the right-hand operand first", "[core][math][mat4]")
{
    // scale-then-translate: (T * S) * p == T * (S * p)
    const Mat4 s = Mat4::scale(Vec3{2, 2, 2});
    const Mat4 t = Mat4::translation(Vec3{5, 0, 0});
    const Mat4 combined = t * s;

    const Vec3 direct = t.transformPoint(s.transformPoint(Vec3{1, 1, 1}));
    const Vec3 viaCombined = combined.transformPoint(Vec3{1, 1, 1});
    CHECK(nearlyEqual(direct, viaCombined));
    CHECK(nearlyEqual(viaCombined, Vec3{7, 2, 2}));
}

TEST_CASE("Mat4 lookAt places eye-space forward along -Z", "[core][math][mat4]")
{
    const Mat4 view = Mat4::lookAt(Vec3{0, 0, 5}, Vec3{0, 0, 0}, Vec3{0, 1, 0});

    // The eye itself must map to the origin of eye space.
    CHECK(nearlyEqual(view.transformPoint(Vec3{0, 0, 5}), Vec3{0, 0, 0}, 1e-4f));

    // A point further along the view direction has a more negative eye-space Z.
    const Vec3 ahead = view.transformPoint(Vec3{0, 0, 0});
    CHECK(ahead.z < 0.0f);
}

TEST_CASE("Mat4 perspective maps near/far plane centers to NDC z = -1/+1", "[core][math][mat4]")
{
    const float fovY = std::numbers::pi_v<float> / 2.0f;
    const Mat4 proj = Mat4::perspective(fovY, 1.0f, 1.0f, 100.0f);

    const Vec3 nearPoint = proj.transformPoint(Vec3{0, 0, -1.0f});
    const Vec3 farPoint = proj.transformPoint(Vec3{0, 0, -100.0f});

    CHECK(nearPoint.z == Approx(-1.0f).margin(1e-4));
    CHECK(farPoint.z == Approx(1.0f).margin(1e-4));
}
