// Exercises the actual exported C ABI (SpatialUnity_*) that Unity's C#
// scripts call via [DllImport] — not just the C++ classes behind it — since
// that boundary (opaque handles, flat structs, caller-allocated output
// buffers) is exactly what could go wrong without a real IDE/Editor to
// catch mismatches. See examples/UnityDemo/README.md for the full picture.

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"
#include "spatial/data/DatasetSerializer.h"
#include "spatial/data/TileSerializer.h"

#include "ProceduralCity.h"
#include "SpatialUnityPlugin.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::tools;

namespace
{
    std::filesystem::path writeUnityTestDataset(const std::string& dirName, std::uint32_t gridSize = 2, float tileSize = 50.0f)
    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / dirName;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "tiles");

        DatasetManifest manifest{};
        manifest.name = "SpatialUnityTestCity";
        manifest.tileSize = tileSize;
        manifest.worldSize = tileSize * static_cast<float>(gridSize);
        manifest.maxLOD = 1;
        manifest.worldHeightMin = 0.0f;
        manifest.worldHeightMax = 20.0f;

        ProceduralCityConfig cityConfig{};
        cityConfig.buildingsPerTileSide = 2;
        cityConfig.seed = 11;

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

    SpatialUnityLoadConfig makeConfig(bool debugVisualization = true)
    {
        SpatialUnityLoadConfig config{};
        config.streamingRadius = 1000.0f;
        config.maxResidentTiles = 256;
        config.cpuMemoryBudgetBytes = 512ull * 1024 * 1024;
        config.workerThreadCount = 2;
        config.maxGPUUploadsPerUpdate = 64;
        config.debugVisualizationEnabled = debugVisualization ? 1 : 0;
        return config;
    }

    SpatialUnityCameraParams cameraAtOrigin()
    {
        SpatialUnityCameraParams camera{};
        camera.posX = camera.posY = camera.posZ = 0.0f;
        camera.fwdX = 0.0f;
        camera.fwdY = 0.0f;
        camera.fwdZ = -1.0f;
        camera.verticalFovRadians = 1.0f;
        camera.viewportHeightPx = 1080.0f;
        return camera;
    }

    // Drives Update() until residentCount reaches expectedResident (CPU tile
    // data loaded), then a few more Update()+Render() passes so throttled
    // GPU uploads finish draining before a test inspects draw commands.
    void streamUntilResident(SpatialUnityWorldHandle world, const SpatialUnityCameraParams& camera, std::uint64_t expectedResident)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
        SpatialUnityStatistics stats{};
        do
        {
            SpatialUnity_Update(world, camera);
            SpatialUnity_GetStatistics(world, &stats);
            if (stats.residentCount == expectedResident)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);

        REQUIRE(stats.residentCount == expectedResident);

        for (int i = 0; i < 30; ++i)
        {
            SpatialUnity_Update(world, camera);
        }
    }
}

TEST_CASE("SpatialUnity_CreateWorld/DestroyWorld round-trips cleanly", "[unity]")
{
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();
    REQUIRE(world != nullptr);
    CHECK(SpatialUnity_IsLoaded(world) == 0);
    SpatialUnity_DestroyWorld(world);
}

TEST_CASE("SpatialUnity_LoadDataset reports DatasetNotFound for a missing manifest", "[unity]")
{
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();
    const SpatialUnityResult result = SpatialUnity_LoadDataset(world, "does/not/exist.world", makeConfig());
    CHECK(result == SpatialUnityResult_DatasetNotFound);
    CHECK(SpatialUnity_IsLoaded(world) == 0);
    SpatialUnity_DestroyWorld(world);
}

TEST_CASE("SpatialUnity_LoadDataset succeeds and exposes dataset max LOD", "[unity]")
{
    const auto manifestPath = writeUnityTestDataset("spatial_unity_test_basic");
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();

    const SpatialUnityResult result = SpatialUnity_LoadDataset(world, manifestPath.string().c_str(), makeConfig());
    REQUIRE(result == SpatialUnityResult_Ok);
    CHECK(SpatialUnity_IsLoaded(world) == 1);
    CHECK(SpatialUnity_GetDatasetMaxLOD(world) == 1);

    SpatialUnity_DestroyWorld(world);
}

TEST_CASE("SpatialUnity streams tiles resident and exposes drawable geometry", "[unity]")
{
    const auto manifestPath = writeUnityTestDataset("spatial_unity_test_stream");
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();
    REQUIRE(SpatialUnity_LoadDataset(world, manifestPath.string().c_str(), makeConfig()) == SpatialUnityResult_Ok);

    const SpatialUnityCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4); // 2x2 grid

    SpatialUnity_Render(world, camera);

    const int32_t drawCount = SpatialUnity_GetDrawCommandCount(world);
    REQUIRE(drawCount > 0);

    std::vector<int64_t> meshIds(static_cast<std::size_t>(drawCount));
    std::vector<int64_t> materialIds(static_cast<std::size_t>(drawCount));
    std::vector<float> transforms(static_cast<std::size_t>(drawCount) * 16);
    SpatialUnity_GetDrawCommands(world, meshIds.data(), materialIds.data(), transforms.data());

    // Every draw command's transform is identity today — SpatialWorld never
    // passes anything else — so this doubles as a check that the transform
    // marshaling (row-major, 16 floats per command) is wired correctly.
    for (int32_t i = 0; i < drawCount; ++i)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                const float expected = (row == col) ? 1.0f : 0.0f;
                CHECK(transforms[(static_cast<std::size_t>(i) * 16) + (row * 4) + col] == expected);
            }
        }
    }

    const int64_t firstMeshId = meshIds[0];
    const int32_t vertexCount = SpatialUnity_GetMeshVertexCount(world, firstMeshId);
    const int32_t indexCount = SpatialUnity_GetMeshIndexCount(world, firstMeshId);
    REQUIRE(vertexCount > 0);
    REQUIRE(indexCount > 0);
    CHECK(indexCount % 3 == 0);

    std::vector<float> positions(static_cast<std::size_t>(vertexCount) * 3);
    std::vector<float> normals(static_cast<std::size_t>(vertexCount) * 3);
    std::vector<float> uvs(static_cast<std::size_t>(vertexCount) * 2);
    std::vector<int32_t> indices(static_cast<std::size_t>(indexCount));
    CHECK(SpatialUnity_GetMeshData(world, firstMeshId, positions.data(), normals.data(), uvs.data(), indices.data()) == 1);

    for (const int32_t index : indices)
    {
        CHECK(index >= 0);
        CHECK(index < vertexCount);
    }

    const int64_t firstMaterialId = materialIds[0];
    if (firstMaterialId != 0)
    {
        float rgba[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
        CHECK(SpatialUnity_GetMaterialColor(world, firstMaterialId, rgba) == 1);
        CHECK(rgba[3] >= 0.0f); // alpha was actually written
    }

    // An id nothing ever created must fail cleanly, not read garbage.
    CHECK(SpatialUnity_GetMeshVertexCount(world, 999999) == 0);
    float unusedRgba[4]{};
    CHECK(SpatialUnity_GetMaterialColor(world, 999999, unusedRgba) == 0);

    SpatialUnity_DestroyWorld(world);
}

TEST_CASE("SpatialUnity debug line output follows the visualization toggle", "[unity]")
{
    const auto manifestPath = writeUnityTestDataset("spatial_unity_test_debug");
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();
    REQUIRE(SpatialUnity_LoadDataset(world, manifestPath.string().c_str(), makeConfig(/*debugVisualization=*/false)) == SpatialUnityResult_Ok);
    CHECK(SpatialUnity_GetDebugVisualization(world) == 0);

    const SpatialUnityCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4);

    SpatialUnity_Render(world, camera);
    CHECK(SpatialUnity_GetDebugLineVertexCount(world) == 0);

    SpatialUnity_SetDebugVisualization(world, 1);
    CHECK(SpatialUnity_GetDebugVisualization(world) == 1);

    SpatialUnity_Render(world, camera);
    const int32_t lineVertexCount = SpatialUnity_GetDebugLineVertexCount(world);
    REQUIRE(lineVertexCount > 0);
    CHECK(lineVertexCount % 2 == 0); // line list: consecutive pairs

    std::vector<float> positions(static_cast<std::size_t>(lineVertexCount) * 3);
    std::vector<float> colors(static_cast<std::size_t>(lineVertexCount) * 4);
    SpatialUnity_GetDebugLineData(world, positions.data(), colors.data());
    CHECK(colors[3] > 0.0f); // alpha of the first vertex was actually written

    SpatialUnity_DestroyWorld(world);
}

TEST_CASE("SpatialUnity_Shutdown releases resources and resets loaded state", "[unity]")
{
    const auto manifestPath = writeUnityTestDataset("spatial_unity_test_shutdown");
    SpatialUnityWorldHandle world = SpatialUnity_CreateWorld();
    REQUIRE(SpatialUnity_LoadDataset(world, manifestPath.string().c_str(), makeConfig()) == SpatialUnityResult_Ok);

    const SpatialUnityCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4);
    SpatialUnity_Render(world, camera);
    REQUIRE(SpatialUnity_GetDrawCommandCount(world) > 0);

    SpatialUnity_Shutdown(world);
    CHECK(SpatialUnity_IsLoaded(world) == 0);

    SpatialUnity_Render(world, camera);
    CHECK(SpatialUnity_GetDrawCommandCount(world) == 0);

    SpatialUnity_DestroyWorld(world);
}
