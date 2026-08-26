#include <fstream>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/data/DatasetSerializer.h"

using namespace spatial;
using namespace spatial::data;
using namespace spatial::core;
using Catch::Approx;

namespace
{
    std::filesystem::path tempFilePath(const std::string& name)
    {
        return std::filesystem::temp_directory_path() / ("spatial_sdk_test_" + name);
    }

    void writeRaw(const std::filesystem::path& path, const std::string& contents)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << contents;
    }
}

TEST_CASE("DatasetSerializer round-trips a manifest through disk", "[data][serialization][dataset]")
{
    DatasetManifest original{};
    original.version = 1;
    original.name = "ExampleCity";
    original.tileSize = 100.0f;
    original.worldSize = 10000.0f;
    original.maxLOD = 4;
    original.coordinateSystem = CoordinateSystem::LocalCartesian;
    original.metadata.set("author", "SpatialTileBuilder");

    const auto path = tempFilePath("roundtrip.world");
    REQUIRE(DatasetSerializer::saveManifest(original, path).hasValue());

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE(result.hasValue());
    const DatasetManifest& loaded = result.value();

    CHECK(loaded.version == original.version);
    CHECK(loaded.name == original.name);
    CHECK(loaded.tileSize == Approx(original.tileSize));
    CHECK(loaded.worldSize == Approx(original.worldSize));
    CHECK(loaded.maxLOD == original.maxLOD);
    CHECK(loaded.coordinateSystem == original.coordinateSystem);
    CHECK(loaded.metadata.getString("author") == std::optional<std::string>("SpatialTileBuilder"));

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer parses the exact example manifest from the spec", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("example.world");
    writeRaw(path, R"({
        "version": 1,
        "name": "ExampleCity",
        "tileSize": 100.0,
        "worldSize": 10000.0,
        "maxLOD": 4,
        "coordinateSystem": "LOCAL_CARTESIAN"
    })");

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE(result.hasValue());
    CHECK(result.value().name == "ExampleCity");
    CHECK(result.value().maxLOD == 4);

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer::loadManifest reports DatasetNotFound for a missing file", "[data][serialization][dataset]")
{
    const auto result = DatasetSerializer::loadManifest(tempFilePath("missing.world"));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::DatasetNotFound);
}

TEST_CASE("DatasetSerializer::loadManifest rejects malformed JSON", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("malformed.world");
    writeRaw(path, "{ this is not valid json ");

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer::loadManifest rejects a manifest missing a required field", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("missing_field.world");
    writeRaw(path, R"({
        "version": 1,
        "tileSize": 100.0,
        "worldSize": 10000.0,
        "maxLOD": 4,
        "coordinateSystem": "LOCAL_CARTESIAN"
    })"); // "name" omitted

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer::loadManifest rejects an unrecognized coordinate system", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("bad_coordsystem.world");
    writeRaw(path, R"({
        "version": 1,
        "name": "ExampleCity",
        "tileSize": 100.0,
        "worldSize": 10000.0,
        "maxLOD": 4,
        "coordinateSystem": "GEOREFERENCED_UTM"
    })");

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::InvalidDataset);

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer::loadManifest rejects an unsupported future version", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("future_version.world");
    writeRaw(path, R"({
        "version": 999,
        "name": "ExampleCity",
        "tileSize": 100.0,
        "worldSize": 10000.0,
        "maxLOD": 4,
        "coordinateSystem": "LOCAL_CARTESIAN"
    })");

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::UnsupportedVersion);

    std::filesystem::remove(path);
}

TEST_CASE("DatasetSerializer::loadManifest ignores unrecognized top-level keys", "[data][serialization][dataset]")
{
    const auto path = tempFilePath("forward_compat.world");
    writeRaw(path, R"({
        "version": 1,
        "name": "ExampleCity",
        "tileSize": 100.0,
        "worldSize": 10000.0,
        "maxLOD": 4,
        "coordinateSystem": "LOCAL_CARTESIAN",
        "someFutureField": { "nested": true }
    })");

    const auto result = DatasetSerializer::loadManifest(path);
    REQUIRE(result.hasValue());
    CHECK(result.value().name == "ExampleCity");

    std::filesystem::remove(path);
}

TEST_CASE("DatasetManifest::worldBounds is centered on the origin", "[data][serialization][dataset]")
{
    DatasetManifest manifest{};
    manifest.worldSize = 200.0f;

    const AABB bounds = manifest.worldBounds();
    CHECK(bounds.min.x == Approx(-100.0f));
    CHECK(bounds.max.x == Approx(100.0f));
    CHECK(bounds.min.z == Approx(-100.0f));
    CHECK(bounds.max.z == Approx(100.0f));
}
