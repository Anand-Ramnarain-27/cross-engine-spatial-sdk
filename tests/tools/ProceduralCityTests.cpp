#include <catch2/catch_test_macros.hpp>

#include "ProceduralCity.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::tools;

namespace
{
    AABB testTileBounds()
    {
        return AABB{Vec3{0, 0, 0}, Vec3{100, 60, 100}};
    }
}

TEST_CASE("generateProceduralTile sets id and bounds", "[tools][proceduralcity]")
{
    const TileId id{2, 3, 5};
    const AABB bounds = testTileBounds();

    const Tile tile = generateProceduralTile(id, bounds, ProceduralCityConfig{}, 0);

    CHECK(tile.id() == id);
    CHECK(tile.bounds().min == bounds.min);
    CHECK(tile.bounds().max == bounds.max);
}

TEST_CASE("generateProceduralTile produces ground and building materials", "[tools][proceduralcity]")
{
    const Tile tile = generateProceduralTile(TileId{0, 0, 0}, testTileBounds(), ProceduralCityConfig{}, 0);

    REQUIRE(tile.materials().size() == 2);
    CHECK(tile.materials()[0].name == "Ground");
    CHECK(tile.materials()[1].name == "Building");
}

TEST_CASE("generateProceduralTile generates one mesh per material at LOD 0", "[tools][proceduralcity]")
{
    ProceduralCityConfig config{};
    config.buildingsPerTileSide = 2; // 4 buildings

    const Tile tile = generateProceduralTile(TileId{0, 0, 0}, testTileBounds(), config, 0);

    REQUIRE(tile.lods().size() == 1);
    const TileLOD& lod0 = tile.lods()[0];
    REQUIRE(lod0.meshes.size() == 2); // ground + buildings
    CHECK(lod0.geometricError == 0.0f);

    const Mesh& ground = lod0.meshes[0];
    CHECK(ground.materialIndex == 0);
    CHECK(ground.triangleCount() == 2); // one quad

    const Mesh& buildings = lod0.meshes[1];
    CHECK(buildings.materialIndex == 1);
    CHECK(buildings.triangleCount() == 4 * 12); // 4 buildings x 12 triangles/box
}

TEST_CASE("generateProceduralTile thins buildings at higher LODs", "[tools][proceduralcity]")
{
    ProceduralCityConfig config{};
    config.buildingsPerTileSide = 4; // 16 buildings

    const Tile tile = generateProceduralTile(TileId{0, 0, 0}, testTileBounds(), config, 2);

    REQUIRE(tile.lods().size() == 3);

    auto buildingTriangleCount = [](const TileLOD& lod) -> std::size_t {
        for (const Mesh& mesh : lod.meshes)
        {
            if (mesh.materialIndex == 1)
            {
                return mesh.triangleCount();
            }
        }
        return 0;
    };

    const std::size_t lod0Triangles = buildingTriangleCount(tile.lods()[0]);
    const std::size_t lod1Triangles = buildingTriangleCount(tile.lods()[1]);
    const std::size_t lod2Triangles = buildingTriangleCount(tile.lods()[2]);

    CHECK(lod0Triangles == 16 * 12);
    CHECK(lod1Triangles < lod0Triangles);
    CHECK(lod2Triangles < lod1Triangles);

    CHECK(tile.lods()[0].geometricError == 0.0f);
    CHECK(tile.lods()[1].geometricError > 0.0f);
    CHECK(tile.lods()[2].geometricError > tile.lods()[1].geometricError);
}

TEST_CASE("generateProceduralTile is deterministic for a given seed and tile id", "[tools][proceduralcity]")
{
    ProceduralCityConfig config{};
    config.seed = 7;

    const Tile a = generateProceduralTile(TileId{1, 2, 3}, testTileBounds(), config, 1);
    const Tile b = generateProceduralTile(TileId{1, 2, 3}, testTileBounds(), config, 1);

    REQUIRE(a.lods().size() == b.lods().size());
    for (std::size_t lod = 0; lod < a.lods().size(); ++lod)
    {
        CHECK(a.lods()[lod].meshes == b.lods()[lod].meshes);
    }
}

TEST_CASE("generateProceduralTile varies with tile id for a fixed seed", "[tools][proceduralcity]")
{
    ProceduralCityConfig config{};
    config.seed = 7;

    const Tile a = generateProceduralTile(TileId{1, 0, 0}, testTileBounds(), config, 0);
    const Tile b = generateProceduralTile(TileId{1, 1, 0}, testTileBounds(), config, 0);

    // Different tiles should not generate byte-identical building geometry.
    CHECK_FALSE(a.lods()[0].meshes[1].vertices == b.lods()[0].meshes[1].vertices);
}
