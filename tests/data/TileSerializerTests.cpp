#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "spatial/data/TileSerializer.h"

using namespace spatial;
using namespace spatial::data;
using namespace spatial::core;

namespace
{
    std::filesystem::path tempFilePath(const std::string& name)
    {
        return std::filesystem::temp_directory_path() / ("spatial_sdk_test_" + name);
    }

    Tile makeSampleTile()
    {
        Tile tile(TileId{1, 2, 3});
        tile.setBounds(AABB{Vec3{0, 0, 0}, Vec3{100, 50, 100}});
        tile.setParent(TileId{0, 1, 1});
        tile.addChild(TileId{2, 4, 6});
        tile.addChild(TileId{2, 5, 6});

        Mesh mesh;
        mesh.vertices = {
            Vertex{Vec3{0, 0, 0}, Vec3{0, 1, 0}, Vec2{0, 0}},
            Vertex{Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec2{1, 0}},
            Vertex{Vec3{0, 0, 1}, Vec3{0, 1, 0}, Vec2{0, 1}},
        };
        mesh.indices = {0, 1, 2};
        mesh.materialIndex = 0;
        tile.addLOD(TileLOD{0.0f, {mesh}});

        Material mat;
        mat.name = "Ground";
        mat.baseColorR = 0.2f;
        mat.baseColorG = 0.6f;
        mat.baseColorB = 0.2f;
        tile.setMaterials({mat});

        tile.metadata().set("source", "unit-test");
        return tile;
    }
}

TEST_CASE("TileSerializer round-trips a tile through disk", "[data][serialization][tile]")
{
    const Tile original = makeSampleTile();
    const auto path = tempFilePath("roundtrip.tile");

    const Expected<void> saveResult = TileSerializer::saveTile(original, path);
    REQUIRE(saveResult.hasValue());

    const Expected<Tile> loadResult = TileSerializer::loadTile(path);
    REQUIRE(loadResult.hasValue());

    const Tile& loaded = loadResult.value();
    CHECK(loaded.id() == original.id());
    CHECK(loaded.bounds().min == original.bounds().min);
    CHECK(loaded.bounds().max == original.bounds().max);
    CHECK(loaded.parent() == original.parent());
    CHECK(loaded.children() == original.children());
    REQUIRE(loaded.lods().size() == original.lods().size());
    REQUIRE(loaded.lods()[0].meshes.size() == 1);
    CHECK(loaded.lods()[0].meshes[0].vertices == original.lods()[0].meshes[0].vertices);
    CHECK(loaded.lods()[0].meshes[0].indices == original.lods()[0].meshes[0].indices);
    REQUIRE(loaded.materials().size() == 1);
    CHECK(loaded.materials()[0].name == "Ground");
    CHECK(loaded.metadata().getString("source") == std::optional<std::string>("unit-test"));

    std::filesystem::remove(path);
}

TEST_CASE("TileSerializer::loadTile reports DatasetNotFound-style failure for a missing file", "[data][serialization][tile]")
{
    const auto result = TileSerializer::loadTile(tempFilePath("does_not_exist.tile"));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::TileLoadFailed);
}

TEST_CASE("TileSerializer::loadTile rejects a file with bad magic", "[data][serialization][tile]")
{
    const auto path = tempFilePath("bad_magic.tile");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "NOTATILEFILE";
    }

    const auto result = TileSerializer::loadTile(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::CorruptTile);

    std::filesystem::remove(path);
}

TEST_CASE("TileSerializer::loadTile rejects a truncated file", "[data][serialization][tile]")
{
    const Tile original = makeSampleTile();
    const auto fullPath = tempFilePath("truncate_source.tile");
    const auto truncPath = tempFilePath("truncated.tile");
    REQUIRE(TileSerializer::saveTile(original, fullPath).hasValue());

    {
        std::ifstream in(fullPath, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        REQUIRE(bytes.size() > 20);
        std::ofstream out(truncPath, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), 20); // cut off well before the end
    }

    const auto result = TileSerializer::loadTile(truncPath);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::CorruptTile);

    std::filesystem::remove(fullPath);
    std::filesystem::remove(truncPath);
}

TEST_CASE("TileSerializer::loadTile rejects an unsupported format version", "[data][serialization][tile]")
{
    const Tile original = makeSampleTile();
    const auto path = tempFilePath("future_version.tile");
    REQUIRE(TileSerializer::saveTile(original, path).hasValue());

    {
        // Byte offset 4 is formatVersion, right after the "SPTL" magic.
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        const std::uint32_t futureVersion = 0xFFFFFFFFu;
        file.seekp(4);
        file.write(reinterpret_cast<const char*>(&futureVersion), sizeof(futureVersion));
    }

    const auto result = TileSerializer::loadTile(path);
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().code == ErrorCode::UnsupportedVersion);

    std::filesystem::remove(path);
}
