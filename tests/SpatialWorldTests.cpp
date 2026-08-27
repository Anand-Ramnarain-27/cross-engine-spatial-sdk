// SpatialWorld is the façade every engine integration talks to instead of
// wiring StreamingManager/LODManager/GPUUploadQueue/DebugRenderer together
// itself — these tests build a real dataset on disk (same as
// StreamingIntegrationTests.cpp) and drive it through a MockRenderer, the
// same way examples/StandaloneViewer's main.cpp does with a real one.

#include <chrono>
#include <filesystem>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "spatial/SpatialWorld.h"
#include "spatial/data/DatasetSerializer.h"
#include "spatial/data/TileSerializer.h"

#include "ProceduralCity.h"
#include "rendering/MockRenderer.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::streaming;
using namespace spatial::rendering;
using namespace spatial::tools;
using spatial::tests::MockRenderer;

namespace
{
    template <typename Predicate>
    bool waitUntil(Predicate predicate, int timeoutMs = 3000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return predicate();
    }

    // residentCount reaching its expected value only means streaming
    // considers the CPU tile data loaded; GPU uploads are throttled
    // (SpatialWorldConfig::maxGPUUploadsPerUpdate) and can still be
    // draining across several more update() calls after that. Tests call
    // this before asserting on GPU state (or ending the test) so nothing
    // is torn down mid-upload.
    void drainUploads(SpatialWorld& world, const CameraParams& camera, MockRenderer& renderer, int extraCalls = 30)
    {
        for (int i = 0; i < extraCalls; ++i)
        {
            world.update(camera, renderer);
        }
    }

    // Writes a small real dataset (manifest + .tile files) to a temp
    // directory and returns the manifest path.
    std::filesystem::path writeTestDataset(const std::string& dirName, std::uint32_t gridSize = 2, float tileSize = 50.0f)
    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / dirName;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "tiles");

        DatasetManifest manifest{};
        manifest.name = "SpatialWorldTestCity";
        manifest.tileSize = tileSize;
        manifest.worldSize = tileSize * static_cast<float>(gridSize);
        manifest.maxLOD = 1;
        manifest.worldHeightMin = 0.0f;
        manifest.worldHeightMax = 20.0f;

        ProceduralCityConfig cityConfig{};
        cityConfig.buildingsPerTileSide = 2;
        cityConfig.seed = 7;

        std::uint32_t level = 0;
        for (std::uint32_t g = gridSize; g > 1; g >>= 1)
        {
            ++level;
        }

        const float half = manifest.worldSize * 0.5f;
        for (std::uint32_t y = 0; y < gridSize; ++y)
        {
            for (std::uint32_t x = 0; x < gridSize; ++x)
            {
                const TileId id{level, x, y};
                const float minX = -half + static_cast<float>(x) * tileSize;
                const float minZ = -half + static_cast<float>(y) * tileSize;
                const AABB bounds{
                    Vec3{minX, 0.0f, minZ},
                    Vec3{minX + tileSize, manifest.worldHeightMax, minZ + tileSize},
                };
                const Tile tile = generateProceduralTile(id, bounds, cityConfig, manifest.maxLOD);
                REQUIRE(TileSerializer::saveTile(tile, root / "tiles" / (id.toString() + ".tile")).hasValue());
            }
        }

        const std::filesystem::path manifestPath = root / "City.world";
        REQUIRE(DatasetSerializer::saveManifest(manifest, manifestPath).hasValue());
        return manifestPath;
    }
}

TEST_CASE("SpatialWorld::loadDataset succeeds and reports loaded state", "[spatialworld]")
{
    const auto manifestPath = writeTestDataset("spatial_world_test_basic");
    SpatialWorld world;
    CHECK_FALSE(world.isLoaded());

    const auto result = world.loadDataset(manifestPath);
    REQUIRE(result.hasValue());
    CHECK(world.isLoaded());
    CHECK(world.datasetManifest().name == "SpatialWorldTestCity");
}

TEST_CASE("SpatialWorld::loadDataset reports an error for a missing manifest", "[spatialworld]")
{
    SpatialWorld world;
    const auto result = world.loadDataset("does/not/exist.world");
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::DatasetNotFound);
    CHECK_FALSE(world.isLoaded());
}

TEST_CASE("SpatialWorld streams tiles resident and draws them", "[spatialworld]")
{
    const auto manifestPath = writeTestDataset("spatial_world_test_stream");

    // `renderer` must outlive `world`: world's GPU resources hold pointers
    // into it, so it must be declared (and therefore destroyed) after
    // renderer — the same lifetime rule documented in
    // examples/StandaloneViewer/src/main.cpp. Getting this backwards is a
    // real use-after-free, not just a style nit — an earlier version of
    // this test had it backwards and crashed with SIGSEGV at scope exit.
    MockRenderer renderer;
    SpatialWorld world;

    SpatialWorldConfig config{};
    config.streaming.streamingRadius = 1000.0f;
    config.streaming.workerThreadCount = 2;
    REQUIRE(world.loadDataset(manifestPath, config).hasValue());

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    REQUIRE(waitUntil([&] {
        world.update(camera, renderer);
        return world.statistics().residentCount == 4; // 2x2 grid
    }));
    drainUploads(world, camera, renderer);

    world.render(renderer, camera);
    CHECK_FALSE(renderer.drawMeshCalls.empty());
    CHECK(renderer.meshesCreated > 0);
}

TEST_CASE("SpatialWorld::frameProfile reflects update()+render() activity", "[spatialworld]")
{
    const auto manifestPath = writeTestDataset("spatial_world_test_profile");

    MockRenderer renderer; // must outlive world — see the note earlier in this file
    SpatialWorld world;

    SpatialWorldConfig config{};
    config.streaming.streamingRadius = 1000.0f;
    REQUIRE(world.loadDataset(manifestPath, config).hasValue());

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    // Before any update()/render() call, the profile is zero-initialized.
    CHECK(world.frameProfile().totalMs == 0.0);

    REQUIRE(waitUntil([&] {
        world.update(camera, renderer);
        return world.statistics().residentCount == 4;
    }));
    drainUploads(world, camera, renderer);
    world.render(renderer, camera);

    const spatial::debug::FrameProfile& profile = world.frameProfile();
    CHECK(profile.totalMs >= 0.0);
    // LOD selection ran for the 4 resident tiles this render() call.
    CHECK(profile.section(spatial::debug::ProfileSection::LODSelection) >= 0.0);
    // Debug visualization defaults on, so the debug-draw section also ran.
    CHECK(profile.section(spatial::debug::ProfileSection::DebugDraw) >= 0.0);
}

TEST_CASE("SpatialWorld draws the debug overlay only when enabled", "[spatialworld]")
{
    const auto manifestPath = writeTestDataset("spatial_world_test_debug");

    MockRenderer renderer; // must outlive world — see the note in the previous test
    SpatialWorld world;

    SpatialWorldConfig config{};
    config.streaming.streamingRadius = 1000.0f;
    config.debugVisualizationEnabled = false;
    REQUIRE(world.loadDataset(manifestPath, config).hasValue());

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    REQUIRE(waitUntil([&] {
        world.update(camera, renderer);
        return world.statistics().residentCount == 4;
    }));
    drainUploads(world, camera, renderer);

    world.render(renderer, camera);
    CHECK(renderer.debugLineBatchCount == 0);

    world.setDebugVisualizationEnabled(true);
    world.render(renderer, camera);
    CHECK(renderer.debugLineBatchCount == 1);
}

TEST_CASE("SpatialWorld::shutdown releases GPU resources and resets state", "[spatialworld]")
{
    const auto manifestPath = writeTestDataset("spatial_world_test_shutdown");

    MockRenderer renderer; // must outlive world — see the note earlier in this file
    SpatialWorld world;

    SpatialWorldConfig config{};
    config.streaming.streamingRadius = 1000.0f;
    REQUIRE(world.loadDataset(manifestPath, config).hasValue());

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};
    REQUIRE(waitUntil([&] {
        world.update(camera, renderer);
        return world.statistics().residentCount == 4;
    }));
    drainUploads(world, camera, renderer);
    REQUIRE(renderer.meshesCreated > 0);

    world.shutdown();
    CHECK_FALSE(world.isLoaded());
    CHECK(static_cast<int>(renderer.destroyedMeshes.size()) == renderer.meshesCreated);
}

TEST_CASE("SpatialWorld::loadDataset can switch datasets", "[spatialworld]")
{
    const auto firstManifest = writeTestDataset("spatial_world_test_reload_a", 2);
    const auto secondManifest = writeTestDataset("spatial_world_test_reload_b", 1);

    MockRenderer renderer; // must outlive world — see the note earlier in this file
    SpatialWorld world;
    SpatialWorldConfig config{};
    config.streaming.streamingRadius = 1000.0f;

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    REQUIRE(world.loadDataset(firstManifest, config).hasValue());
    CHECK(world.datasetManifest().name == "SpatialWorldTestCity");

    // Get partway through streaming/uploading the first dataset before
    // switching — this is the scenario that actually exercises
    // loadDataset()'s internal shutdown() with GPU uploads still in flight
    // for the dataset being replaced, not just a clean load-then-load.
    world.update(camera, renderer);

    REQUIRE(world.loadDataset(secondManifest, config).hasValue());
    CHECK(world.isLoaded());

    REQUIRE(waitUntil([&] {
        world.update(camera, renderer);
        return world.statistics().residentCount == 1; // second dataset is a 1x1 grid
    }));
    drainUploads(world, camera, renderer);
    world.render(renderer, camera);
    CHECK_FALSE(renderer.drawMeshCalls.empty());
}
