// End-to-end check that Phase 3 (tile format), Phase 4 (spatial index), and
// Phase 6 (streaming) actually work together against real files on disk —
// not just against the in-memory fakes the other streaming tests use.

#include <chrono>
#include <filesystem>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "spatial/data/DatasetManifest.h"
#include "spatial/data/TileIndex.h"
#include "spatial/data/TileSerializer.h"
#include "spatial/streaming/StreamingManager.h"

#include "ProceduralCity.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::streaming;
using namespace spatial::tools;

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
}

TEST_CASE("StreamingManager streams real tile files generated for a real dataset", "[streaming][integration]")
{
    const std::filesystem::path tilesDir =
        std::filesystem::temp_directory_path() / "spatial_sdk_streaming_integration_test";
    std::filesystem::create_directories(tilesDir);

    DatasetManifest manifest{};
    manifest.name = "IntegrationTestCity";
    manifest.tileSize = 50.0f;
    manifest.worldSize = 100.0f; // 2x2 grid
    manifest.maxLOD = 2;

    ProceduralCityConfig cityConfig{};
    cityConfig.buildingsPerTileSide = 2;
    cityConfig.seed = 99;

    const auto indexResult = TileIndex::buildUniformGrid(manifest);
    REQUIRE(indexResult.hasValue());
    const TileIndex& index = indexResult.value();
    REQUIRE(index.size() == 4);

    // Generate and write real .tile files for every tile the index knows about.
    for (std::uint32_t y = 0; y < 2; ++y)
    {
        for (std::uint32_t x = 0; x < 2; ++x)
        {
            const TileId id{1, x, y}; // level = log2(gridSize=2) = 1
            const auto bounds = index.find(id);
            REQUIRE(bounds.has_value());

            const Tile tile = generateProceduralTile(id, *bounds, cityConfig, manifest.maxLOD);
            const auto saveResult = TileSerializer::saveTile(tile, tilesDir / (id.toString() + ".tile"));
            REQUIRE(saveResult.hasValue());
        }
    }

    StreamingConfig config{};
    config.streamingRadius = 1000.0f; // generous: everything in this tiny dataset is "in range"
    config.workerThreadCount = 2;

    StreamingManager manager(index, makeFileTileLoader(tilesDir), config);

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    const bool allResident = waitUntil([&] {
        manager.update(camera);
        return manager.residentTileIds().size() == 4;
    });
    REQUIRE(allResident);

    for (std::uint32_t y = 0; y < 2; ++y)
    {
        for (std::uint32_t x = 0; x < 2; ++x)
        {
            const TileId id{1, x, y};
            const Tile* tile = manager.residentTile(id);
            REQUIRE(tile != nullptr);
            CHECK(tile->id() == id);
            REQUIRE_FALSE(tile->lods().empty());
            // The generated LOD 0 mesh set includes buildings for a
            // non-trivial ProceduralCityConfig, so this isn't an empty tile.
            CHECK_FALSE(tile->lods()[0].meshes.empty());
        }
    }

    CHECK(manager.statistics().totalLoadsCompleted == 4);
    CHECK(manager.statistics().totalLoadsFailed == 0);

    std::filesystem::remove_all(tilesDir);
}
