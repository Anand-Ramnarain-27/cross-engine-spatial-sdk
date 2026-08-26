#include <catch2/catch_test_macros.hpp>

#include "spatial/data/TileMemory.h"

using namespace spatial::core;
using namespace spatial::data;

TEST_CASE("estimateTileMemoryBytes grows with vertex/index content", "[data][tilememory]")
{
    Tile empty(TileId{0, 0, 0});
    const std::size_t emptyBytes = estimateTileMemoryBytes(empty);
    CHECK(emptyBytes > 0); // baseline struct overhead

    Mesh mesh;
    mesh.vertices = {
        Vertex{Vec3{0, 0, 0}, Vec3{0, 1, 0}, Vec2{0, 0}},
        Vertex{Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec2{1, 0}},
        Vertex{Vec3{0, 0, 1}, Vec3{0, 1, 0}, Vec2{0, 1}},
    };
    mesh.indices = {0, 1, 2};

    Tile withMesh(TileId{0, 0, 0});
    withMesh.addLOD(TileLOD{0.0f, {mesh}});

    const std::size_t withMeshBytes = estimateTileMemoryBytes(withMesh);
    CHECK(withMeshBytes > emptyBytes);

    const std::size_t expectedGeometryBytes =
        mesh.vertices.size() * sizeof(Vertex) + mesh.indices.size() * sizeof(std::uint32_t);
    CHECK(withMeshBytes >= emptyBytes + expectedGeometryBytes);
}

TEST_CASE("estimateTileMemoryBytes grows with materials and metadata", "[data][tilememory]")
{
    Tile tile(TileId{0, 0, 0});
    const std::size_t base = estimateTileMemoryBytes(tile);

    tile.setMaterials({Material{}, Material{}});
    const std::size_t withMaterials = estimateTileMemoryBytes(tile);
    CHECK(withMaterials > base);

    tile.metadata().set("a-fairly-long-metadata-key", "and-a-fairly-long-metadata-value-too");
    const std::size_t withMetadata = estimateTileMemoryBytes(tile);
    CHECK(withMetadata > withMaterials);
}

TEST_CASE("estimateTileMemoryBytes scales roughly linearly with LOD count", "[data][tilememory]")
{
    Mesh mesh;
    mesh.vertices.resize(100);
    mesh.indices.resize(300);

    Tile oneLOD(TileId{0, 0, 0});
    oneLOD.addLOD(TileLOD{0.0f, {mesh}});

    Tile threeLODs(TileId{0, 0, 0});
    threeLODs.addLOD(TileLOD{0.0f, {mesh}});
    threeLODs.addLOD(TileLOD{1.0f, {mesh}});
    threeLODs.addLOD(TileLOD{2.0f, {mesh}});

    const std::size_t oneBytes = estimateTileMemoryBytes(oneLOD);
    const std::size_t threeBytes = estimateTileMemoryBytes(threeLODs);

    // Roughly 3x the geometry, not exactly (fixed per-tile overhead doesn't scale).
    CHECK(threeBytes > oneBytes * 2);
}
