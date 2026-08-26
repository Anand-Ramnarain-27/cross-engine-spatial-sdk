// End-to-end check that a tile produced by the streaming pipeline (Phase 6)
// can actually be uploaded through the rendering abstraction (Phase 8) —
// not just that each layer works against synthetic data in isolation.

#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "spatial/rendering/GPUUploadQueue.h"
#include "spatial/streaming/StreamingManager.h"

#include "rendering/MockRenderer.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::streaming;
using namespace spatial::rendering;
using spatial::tests::MockRenderer;

namespace
{
    template <typename Predicate>
    bool waitUntil(Predicate predicate, int timeoutMs = 2000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return predicate();
    }

    TileLoader loaderWithTwoMaterialMesh()
    {
        return [](const TileId& id) -> Expected<Tile> {
            Tile tile(id);

            Mesh ground;
            ground.vertices = {Vertex{}, Vertex{}, Vertex{}};
            ground.indices = {0, 1, 2};
            ground.materialIndex = 0;

            Mesh building;
            building.vertices.resize(24);
            building.indices.resize(36);
            building.materialIndex = 1;

            tile.addLOD(TileLOD{0.0f, {ground, building}});
            tile.setMaterials({Material{}, Material{}});
            return tile;
        };
    }
}

TEST_CASE("A resident tile's LOD 0 meshes upload through GPUUploadQueue", "[rendering][integration]")
{
    TileIndex index(AABB{Vec3{-1000, -1000, -1000}, Vec3{1000, 1000, 1000}});
    index.insert(TileId{0, 0, 0}, AABB{Vec3{-5, -5, -5}, Vec3{5, 5, 5}});

    StreamingConfig config{};
    config.streamingRadius = 100.0f;
    StreamingManager manager(index, loaderWithTwoMaterialMesh(), config);

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};
    REQUIRE(waitUntil([&] {
        manager.update(camera);
        return manager.stateOf(TileId{0, 0, 0}) == ResourceState::Resident;
    }));

    const Tile* tile = manager.residentTile(TileId{0, 0, 0});
    REQUIRE(tile != nullptr);
    REQUIRE_FALSE(tile->lods().empty());
    REQUIRE(tile->lods()[0].meshes.size() == 2); // ground + building, per the loader above

    MockRenderer renderer;
    GPUUploadQueue queue;
    std::vector<MeshResource> uploaded;
    for (const Mesh& mesh : tile->lods()[0].meshes)
    {
        queue.enqueueMesh(mesh, [&](MeshResource resource) { uploaded.push_back(std::move(resource)); });
    }
    for (const Material& material : tile->materials())
    {
        queue.enqueueMaterial(material, [](MaterialResource) {});
    }

    const std::size_t processed = queue.processQueue(renderer, 10);

    CHECK(processed == 4); // 2 meshes + 2 materials
    CHECK(uploaded.size() == 2);
    CHECK(renderer.meshesCreated == 2);
    CHECK(renderer.materialsCreated == 2);
    for (const MeshResource& resource : uploaded)
    {
        CHECK(resource.valid());
    }

    // The ground mesh (3 verts) and building mesh (24 verts) both made it
    // through with the right vertex counts, not just the right count of calls.
    CHECK(renderer.meshVertexCounts.at(uploaded[0].handle().id) == 3);
    CHECK(renderer.meshVertexCounts.at(uploaded[1].handle().id) == 24);
}

TEST_CASE("Draw calls submitted after upload reference valid handles", "[rendering][integration]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    Mesh mesh;
    mesh.vertices = {Vertex{}, Vertex{}, Vertex{}};
    mesh.indices = {0, 1, 2};

    // A real cache would own these for the tile's lifetime; keeping them
    // alive here is what makes the handles below still valid to draw with.
    MeshResource meshResource;
    queue.enqueueMesh(mesh, [&](MeshResource resource) { meshResource = std::move(resource); });

    MaterialResource materialResource;
    queue.enqueueMaterial(Material{}, [&](MaterialResource resource) { materialResource = std::move(resource); });

    queue.processQueue(renderer, 10);
    REQUIRE(meshResource.valid());
    REQUIRE(materialResource.valid());

    renderer.beginFrame(Mat4::identity());
    renderer.drawMesh(meshResource.handle(), materialResource.handle(), Mat4::identity());
    renderer.endFrame();

    REQUIRE(renderer.drawMeshCalls.size() == 1);
    CHECK(renderer.drawMeshCalls[0].mesh == meshResource.handle());
    CHECK(renderer.drawMeshCalls[0].material == materialResource.handle());
    CHECK(renderer.beginFrameCount == 1);
    CHECK(renderer.endFrameCount == 1);
    CHECK(renderer.destroyedMeshes.empty()); // still alive: meshResource hasn't gone out of scope
}
