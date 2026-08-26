#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/lod/ScreenSpaceError.h"

using namespace spatial::lod;
using Catch::Approx;

TEST_CASE("computeScreenSpaceError is zero for exact geometry", "[lod][sse]")
{
    CHECK(computeScreenSpaceError(0.0f, 100.0f, std::numbers::pi_v<float> / 3.0f, 1080.0f) == Approx(0.0f));
}

TEST_CASE("computeScreenSpaceError decreases as distance increases", "[lod][sse]")
{
    const float fov = std::numbers::pi_v<float> / 3.0f;
    const float near = computeScreenSpaceError(1.0f, 10.0f, fov, 1080.0f);
    const float far = computeScreenSpaceError(1.0f, 100.0f, fov, 1080.0f);
    CHECK(near > far);
}

TEST_CASE("computeScreenSpaceError increases with geometricError", "[lod][sse]")
{
    const float fov = std::numbers::pi_v<float> / 3.0f;
    const float small = computeScreenSpaceError(1.0f, 100.0f, fov, 1080.0f);
    const float large = computeScreenSpaceError(5.0f, 100.0f, fov, 1080.0f);
    CHECK(large == Approx(small * 5.0f));
}

TEST_CASE("screenSpaceErrorCrossoverDistance is the inverse of computeScreenSpaceError", "[lod][sse]")
{
    const float fov = std::numbers::pi_v<float> / 3.0f;
    const float viewportHeight = 900.0f;
    const float geometricError = 3.5f;
    const float maxErrorPx = 16.0f;

    const float crossover = screenSpaceErrorCrossoverDistance(geometricError, fov, viewportHeight, maxErrorPx);
    const float sseAtCrossover = computeScreenSpaceError(geometricError, crossover, fov, viewportHeight);

    CHECK(sseAtCrossover == Approx(maxErrorPx).margin(1e-3));
}

TEST_CASE("screenSpaceErrorCrossoverDistance is zero for exact geometry", "[lod][sse]")
{
    const float fov = std::numbers::pi_v<float> / 3.0f;
    CHECK(screenSpaceErrorCrossoverDistance(0.0f, fov, 1080.0f, 16.0f) == Approx(0.0f));
}
