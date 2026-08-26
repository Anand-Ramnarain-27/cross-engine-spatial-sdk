#include <numbers>

#include <catch2/catch_test_macros.hpp>

#include "spatial/lod/LODManager.h"
#include "spatial/lod/ScreenSpaceError.h"

using namespace spatial::core;
using namespace spatial::lod;

namespace
{
    LODConfig distanceConfig()
    {
        LODConfig config{};
        config.useScreenSpaceError = false;
        config.distanceThresholds = {100.0f, 300.0f, 800.0f};
        config.hysteresisRatio = 0.1f;
        return config;
    }

    CameraParams cameraAt(float x)
    {
        CameraParams camera{};
        camera.position = Vec3{x, 0.0f, 0.0f};
        return camera;
    }
}

TEST_CASE("LODManager picks the plain distance LOD on first selection", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    const std::uint32_t lod = manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(150.0f));
    CHECK(lod == 1);
    CHECK(manager.currentLOD(1) == std::optional<std::uint32_t>(1));
}

TEST_CASE("LODManager hysteresis holds steady just past a boundary", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(90.0f)) == 0);

    // 105 is past the 100 threshold but within the 10% hysteresis band (< 110).
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(105.0f)) == 0);
}

TEST_CASE("LODManager hysteresis releases once clearly past the boundary", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(90.0f)) == 0);
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(105.0f)) == 0); // held
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(115.0f)) == 1); // released
}

TEST_CASE("LODManager hysteresis applies symmetrically when moving to a finer LOD", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(115.0f)) == 1);

    // 95 is back under the 100 threshold but within the 10% band (> 90).
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(95.0f)) == 1); // held
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(85.0f)) == 0); // released
}

TEST_CASE("LODManager snaps immediately on a jump of more than one LOD", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(50.0f)) == 0);
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(5000.0f)) == 3);
}

TEST_CASE("LODManager tracks hysteresis state independently per key", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(90.0f)) == 0);
    CHECK(manager.selectLOD(2, Vec3{0, 0, 0}, {}, cameraAt(5000.0f)) == 3);

    CHECK(manager.currentLOD(1) == std::optional<std::uint32_t>(0));
    CHECK(manager.currentLOD(2) == std::optional<std::uint32_t>(3));
}

TEST_CASE("LODManager::forget and reset clear hysteresis state", "[lod][lodmanager]")
{
    LODManager<int> manager(distanceConfig());
    manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(90.0f));
    REQUIRE(manager.currentLOD(1).has_value());

    manager.forget(1);
    CHECK_FALSE(manager.currentLOD(1).has_value());

    manager.selectLOD(1, Vec3{0, 0, 0}, {}, cameraAt(90.0f));
    manager.selectLOD(2, Vec3{0, 0, 0}, {}, cameraAt(90.0f));
    manager.reset();
    CHECK_FALSE(manager.currentLOD(1).has_value());
    CHECK_FALSE(manager.currentLOD(2).has_value());
}

TEST_CASE("LODManager screen-space-error mode matches manually computed crossover distances", "[lod][lodmanager]")
{
    LODConfig config{};
    config.useScreenSpaceError = true;
    config.maxScreenSpaceErrorPx = 16.0f;

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};
    camera.verticalFovRadians = std::numbers::pi_v<float> / 3.0f;
    camera.viewportHeightPx = 1080.0f;

    const std::vector<float> geometricErrors = {0.0f, 1.0f, 2.0f, 4.0f};
    const float d1 = screenSpaceErrorCrossoverDistance(1.0f, camera.verticalFovRadians, camera.viewportHeightPx, 16.0f);
    const float d2 = screenSpaceErrorCrossoverDistance(2.0f, camera.verticalFovRadians, camera.viewportHeightPx, 16.0f);

    LODManager<int> manager(config);

    // Just short of d1: finest LOD.
    CameraParams nearCamera = camera;
    nearCamera.position = Vec3{d1 * 0.5f, 0, 0};
    CHECK(manager.selectLOD(1, Vec3{0, 0, 0}, geometricErrors, nearCamera) == 0);

    // Between d1 and d2: LOD 1. Use a fresh key so hysteresis doesn't hold it at 0.
    CameraParams midCamera = camera;
    midCamera.position = Vec3{(d1 + d2) * 0.5f, 0, 0};
    CHECK(manager.selectLOD(2, Vec3{0, 0, 0}, geometricErrors, midCamera) == 1);
}
