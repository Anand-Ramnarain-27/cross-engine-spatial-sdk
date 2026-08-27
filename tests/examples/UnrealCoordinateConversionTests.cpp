// The single most error-prone part of the Unreal plugin — right-handed
// Y-up meters <-> Unreal's left-handed Z-up centimeters — tested as pure
// math, independent of the full plugin/SpatialWorld machinery. See
// UnrealCoordinateConversion.h's header comment for the derivation this
// verifies.

#include <catch2/catch_test_macros.hpp>

#include "UnrealCoordinateConversion.h"

using namespace spatial::core;
namespace conv = spatial::examples::unreal_convert;

TEST_CASE("unreal_convert::position maps SDK up (Y) to Unreal up (Z), scaled to centimeters", "[unreal][coordinateconversion]")
{
    float x, y, z;
    conv::position(0.0f, 1.0f, 0.0f, x, y, z);
    CHECK(x == 0.0f);
    CHECK(y == 0.0f);
    CHECK(z == 100.0f); // 1m -> 100cm, landed on Unreal's Z (up) axis
}

TEST_CASE("unreal_convert::position maps SDK Z to Unreal Y, scaled to centimeters", "[unreal][coordinateconversion]")
{
    float x, y, z;
    conv::position(0.0f, 0.0f, 1.0f, x, y, z);
    CHECK(x == 0.0f);
    CHECK(y == 100.0f);
    CHECK(z == 0.0f);
}

TEST_CASE("unreal_convert::position leaves X unchanged apart from scale", "[unreal][coordinateconversion]")
{
    float x, y, z;
    conv::position(3.0f, 0.0f, 0.0f, x, y, z);
    CHECK(x == 300.0f);
    CHECK(y == 0.0f);
    CHECK(z == 0.0f);
}

TEST_CASE("unreal_convert::position/toSpatialPosition round-trip", "[unreal][coordinateconversion]")
{
    float ux, uy, uz;
    conv::position(1.5f, -2.25f, 7.0f, ux, uy, uz);
    const Vec3 back = conv::toSpatialPosition(ux, uy, uz);
    CHECK(back.x == 1.5f);
    CHECK(back.y == -2.25f);
    CHECK(back.z == 7.0f);
}

TEST_CASE("unreal_convert::direction is not scaled, only permuted", "[unreal][coordinateconversion]")
{
    float x, y, z;
    conv::direction(0.0f, 1.0f, 0.0f, x, y, z); // SDK "up"
    CHECK(x == 0.0f);
    CHECK(y == 0.0f);
    CHECK(z == 1.0f); // Unreal "up", unit length preserved (no *100)
}

TEST_CASE("unreal_convert::direction/toSpatialDirection round-trip", "[unreal][coordinateconversion]")
{
    float ux, uy, uz;
    conv::direction(0.6f, 0.8f, 0.0f, ux, uy, uz);
    const Vec3 back = conv::toSpatialDirection(ux, uy, uz);
    CHECK(back.x == 0.6f);
    CHECK(back.y == 0.8f);
    CHECK(back.z == 0.0f);
}

TEST_CASE("unreal_convert::transform maps identity to identity", "[unreal][coordinateconversion]")
{
    const Mat4 result = conv::transform(Mat4::identity());
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            CHECK(result.m[row][col] == (row == col ? 1.0f : 0.0f));
        }
    }
}

TEST_CASE("unreal_convert::transform scales and permutes a pure translation", "[unreal][coordinateconversion]")
{
    const Mat4 translation = Mat4::translation(Vec3{1.0f, 2.0f, 3.0f}); // SDK: right 1m, up 2m, Z 3m
    const Mat4 result = conv::transform(translation);

    // Rotation/scale block is still identity — only the translation column changed.
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            CHECK(result.m[row][col] == (row == col ? 1.0f : 0.0f));
        }
    }

    CHECK(result.m[0][3] == 100.0f); // X unchanged, scaled
    CHECK(result.m[1][3] == 300.0f); // Unreal Y <- SDK Z (3m), scaled
    CHECK(result.m[2][3] == 200.0f); // Unreal Z <- SDK Y (2m), scaled
    CHECK(result.m[3][3] == 1.0f);
}
