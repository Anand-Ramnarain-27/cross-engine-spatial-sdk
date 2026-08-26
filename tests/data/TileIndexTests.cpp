#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/data/TileIndex.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using Catch::Approx;

namespace
{
    DatasetManifest makeManifest(float tileSize, float worldSize)
    {
        DatasetManifest manifest{};
        manifest.name = "TestCity";
        manifest.tileSize = tileSize;
        manifest.worldSize = worldSize;
        manifest.maxLOD = 2;
        return manifest;
    }
}

TEST_CASE("TileIndex::buildUniformGrid produces gridSize^2 tiles at the expected level", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(100.0f, 400.0f)); // 4x4 grid
    REQUIRE(result.hasValue());
    CHECK(result.value().size() == 16);
}

TEST_CASE("TileIndex::find returns the exact bounds for a known tile", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(100.0f, 400.0f));
    REQUIRE(result.hasValue());
    const TileIndex& index = result.value();

    // Level = log2(4) = 2. Tile (0, 0) sits at the world-bounds minimum corner.
    const auto bounds = index.find(TileId{2, 0, 0});
    REQUIRE(bounds.has_value());
    CHECK(bounds->min.x == Approx(-200.0f));
    CHECK(bounds->min.z == Approx(-200.0f));
    CHECK(bounds->max.x == Approx(-100.0f));
    CHECK(bounds->max.z == Approx(-100.0f));

    CHECK_FALSE(index.find(TileId{2, 99, 99}).has_value());
}

TEST_CASE("TileIndex::queryRadius finds nearby tiles and excludes far ones", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(100.0f, 400.0f));
    REQUIRE(result.hasValue());
    const TileIndex& index = result.value();

    const std::vector<TileId> nearOrigin = index.queryRadius(Vec3{0, 0, 0}, 60.0f);
    CHECK_FALSE(nearOrigin.empty());

    const std::vector<TileId> farAway = index.queryRadius(Vec3{10000, 0, 10000}, 1.0f);
    CHECK(farAway.empty());
}

TEST_CASE("TileIndex::buildUniformGrid rejects a non-power-of-two grid", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(100.0f, 300.0f)); // 3x3 grid
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);
}

TEST_CASE("TileIndex::buildUniformGrid rejects worldSize not a multiple of tileSize", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(100.0f, 350.0f));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);
}

TEST_CASE("TileIndex::buildUniformGrid rejects non-positive sizes", "[data][tileindex]")
{
    const auto result = TileIndex::buildUniformGrid(makeManifest(0.0f, 400.0f));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);
}
