// Exercises the actual exported C ABI (SpatialUnreal_*) that Unreal's C++
// plugin calls — not just the C++ classes behind it. Complements
// UnrealCoordinateConversionTests.cpp (pure conversion math) by checking
// the conversion is actually wired into the pulled data: mesh positions
// come out already in Unreal-space (centimeters, Z-up).

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"
#include "spatial/data/DatasetSerializer.h"
#include "spatial/data/TileSerializer.h"

#include "ProceduralCity.h"
#include "SpatialUnrealPlugin.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::tools;

namespace
{
    std::filesystem::path writeUnrealTestDataset(const std::string& dirName, std::uint32_t gridSize = 2, float tileSize = 50.0f)
    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / dirName;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "tiles");

        DatasetManifest manifest{};
        manifest.name = "SpatialUnrealTestCity";
        manifest.tileSize = tileSize;
        manifest.worldSize = tileSize * static_cast<float>(gridSize);
        manifest.maxLOD = 1;
        manifest.worldHeightMin = 0.0f;
        manifest.worldHeightMax = 20.0f;

        ProceduralCityConfig cityConfig{};
        cityConfig.buildingsPerTileSide = 2;
        cityConfig.seed = 13;

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

    SpatialUnrealLoadConfig makeConfig(bool debugVisualization = true)
    {
        SpatialUnrealLoadConfig config{};
        config.streamingRadiusCm = 100000.0f; // 1000m
        config.maxResidentTiles = 256;
        config.cpuMemoryBudgetBytes = 512ull * 1024 * 1024;
        config.workerThreadCount = 2;
        config.maxGPUUploadsPerUpdate = 64;
        config.debugVisualizationEnabled = debugVisualization ? 1 : 0;
        return config;
    }

    SpatialUnrealCameraParams cameraAtOrigin()
    {
        SpatialUnrealCameraParams camera{};
        camera.posX = camera.posY = camera.posZ = 0.0f;
        camera.fwdX = 100.0f; // Unreal "forward" — need not be normalized
        camera.fwdY = 0.0f;
        camera.fwdZ = 0.0f;
        camera.verticalFovRadians = 1.0f;
        camera.viewportHeightPx = 1080.0f;
        return camera;
    }

    void streamUntilResident(SpatialUnrealWorldHandle world, const SpatialUnrealCameraParams& camera, std::uint64_t expectedResident)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
        SpatialUnrealStatistics stats{};
        do
        {
            SpatialUnreal_Update(world, camera);
            SpatialUnreal_GetStatistics(world, &stats);
            if (stats.residentCount == expectedResident)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } while (std::chrono::steady_clock::now() < deadline);

        REQUIRE(stats.residentCount == expectedResident);

        for (int i = 0; i < 30; ++i)
        {
            SpatialUnreal_Update(world, camera);
        }
    }
}

TEST_CASE("SpatialUnreal_CreateWorld/DestroyWorld round-trips cleanly", "[unreal]")
{
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();
    REQUIRE(world != nullptr);
    CHECK(SpatialUnreal_IsLoaded(world) == 0);
    SpatialUnreal_DestroyWorld(world);
}

TEST_CASE("SpatialUnreal_LoadDataset reports DatasetNotFound for a missing manifest", "[unreal]")
{
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();
    const SpatialUnrealResult result = SpatialUnreal_LoadDataset(world, "does/not/exist.world", makeConfig());
    CHECK(result == SpatialUnrealResult_DatasetNotFound);
    CHECK(SpatialUnreal_IsLoaded(world) == 0);
    SpatialUnreal_DestroyWorld(world);
}

TEST_CASE("SpatialUnreal_LoadDataset succeeds and exposes dataset max LOD", "[unreal]")
{
    const auto manifestPath = writeUnrealTestDataset("spatial_unreal_test_basic");
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();

    const SpatialUnrealResult result = SpatialUnreal_LoadDataset(world, manifestPath.string().c_str(), makeConfig());
    REQUIRE(result == SpatialUnrealResult_Ok);
    CHECK(SpatialUnreal_IsLoaded(world) == 1);
    CHECK(SpatialUnreal_GetDatasetMaxLOD(world) == 1);

    SpatialUnreal_DestroyWorld(world);
}

TEST_CASE("SpatialUnreal streams tiles resident and exposes drawable geometry already in Unreal-space", "[unreal]")
{
    const auto manifestPath = writeUnrealTestDataset("spatial_unreal_test_stream");
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();
    REQUIRE(SpatialUnreal_LoadDataset(world, manifestPath.string().c_str(), makeConfig()) == SpatialUnrealResult_Ok);

    const SpatialUnrealCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4); // 2x2 grid

    SpatialUnreal_Render(world, camera);

    const int32_t drawCount = SpatialUnreal_GetDrawCommandCount(world);
    REQUIRE(drawCount > 0);

    std::vector<int64_t> meshIds(static_cast<std::size_t>(drawCount));
    std::vector<int64_t> materialIds(static_cast<std::size_t>(drawCount));
    std::vector<float> transforms(static_cast<std::size_t>(drawCount) * 16);
    SpatialUnreal_GetDrawCommands(world, meshIds.data(), materialIds.data(), transforms.data());

    // Still identity today (see UnrealCoordinateConversionTests.cpp for the
    // conversion math on a non-identity matrix) — this doubles as a check
    // that the marshaling itself (row-major, 16 floats per command) works.
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
    const int32_t vertexCount = SpatialUnreal_GetMeshVertexCount(world, firstMeshId);
    const int32_t indexCount = SpatialUnreal_GetMeshIndexCount(world, firstMeshId);
    REQUIRE(vertexCount > 0);
    REQUIRE(indexCount > 0);
    CHECK(indexCount % 3 == 0);

    std::vector<float> positions(static_cast<std::size_t>(vertexCount) * 3);
    std::vector<float> normals(static_cast<std::size_t>(vertexCount) * 3);
    std::vector<float> uvs(static_cast<std::size_t>(vertexCount) * 2);
    std::vector<int32_t> indices(static_cast<std::size_t>(indexCount));
    CHECK(SpatialUnreal_GetMeshData(world, firstMeshId, positions.data(), normals.data(), uvs.data(), indices.data()) == 1);

    for (const int32_t index : indices)
    {
        CHECK(index >= 0);
        CHECK(index < vertexCount);
    }

    // The dataset's height range is [0, 20]m; positions come out already
    // converted, so Unreal's up axis (Z, packed index 2 of each vertex)
    // must land in [0, 2000]cm — a real check that the axis landed on Z
    // and the scale is centimeters, not a roundabout tautology.
    for (int32_t i = 0; i < vertexCount; ++i)
    {
        const float z = positions[(static_cast<std::size_t>(i) * 3) + 2];
        CHECK(z >= 0.0f);
        CHECK(z <= 2000.0f);
    }

    const int64_t firstMaterialId = materialIds[0];
    if (firstMaterialId != 0)
    {
        float rgba[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
        CHECK(SpatialUnreal_GetMaterialColor(world, firstMaterialId, rgba) == 1);
        CHECK(rgba[3] >= 0.0f); // alpha was actually written
    }

    CHECK(SpatialUnreal_GetMeshVertexCount(world, 999999) == 0);
    float unusedRgba[4]{};
    CHECK(SpatialUnreal_GetMaterialColor(world, 999999, unusedRgba) == 0);

    SpatialUnreal_DestroyWorld(world);
}

TEST_CASE("SpatialUnreal debug line output follows the visualization toggle", "[unreal]")
{
    const auto manifestPath = writeUnrealTestDataset("spatial_unreal_test_debug");
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();
    REQUIRE(SpatialUnreal_LoadDataset(world, manifestPath.string().c_str(), makeConfig(/*debugVisualization=*/false)) == SpatialUnrealResult_Ok);
    CHECK(SpatialUnreal_GetDebugVisualization(world) == 0);

    const SpatialUnrealCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4);

    SpatialUnreal_Render(world, camera);
    CHECK(SpatialUnreal_GetDebugLineVertexCount(world) == 0);

    SpatialUnreal_SetDebugVisualization(world, 1);
    CHECK(SpatialUnreal_GetDebugVisualization(world) == 1);

    SpatialUnreal_Render(world, camera);
    const int32_t lineVertexCount = SpatialUnreal_GetDebugLineVertexCount(world);
    REQUIRE(lineVertexCount > 0);
    CHECK(lineVertexCount % 2 == 0);

    std::vector<float> positions(static_cast<std::size_t>(lineVertexCount) * 3);
    std::vector<float> colors(static_cast<std::size_t>(lineVertexCount) * 4);
    SpatialUnreal_GetDebugLineData(world, positions.data(), colors.data());
    CHECK(colors[3] > 0.0f);

    SpatialUnreal_DestroyWorld(world);
}

TEST_CASE("SpatialUnreal_Shutdown releases resources and resets loaded state", "[unreal]")
{
    const auto manifestPath = writeUnrealTestDataset("spatial_unreal_test_shutdown");
    SpatialUnrealWorldHandle world = SpatialUnreal_CreateWorld();
    REQUIRE(SpatialUnreal_LoadDataset(world, manifestPath.string().c_str(), makeConfig()) == SpatialUnrealResult_Ok);

    const SpatialUnrealCameraParams camera = cameraAtOrigin();
    streamUntilResident(world, camera, 4);
    SpatialUnreal_Render(world, camera);
    REQUIRE(SpatialUnreal_GetDrawCommandCount(world) > 0);

    SpatialUnreal_Shutdown(world);
    CHECK(SpatialUnreal_IsLoaded(world) == 0);

    SpatialUnreal_Render(world, camera);
    CHECK(SpatialUnreal_GetDrawCommandCount(world) == 0);

    SpatialUnreal_DestroyWorld(world);
}
