#include <catch2/catch_test_macros.hpp>

#include "spatial/rendering/GPUUploadQueue.h"

#include "rendering/MockRenderer.h"

using namespace spatial;
using namespace spatial::rendering;
using spatial::tests::MockRenderer;

TEST_CASE("GPUUploadQueue uploads a mesh and invokes its callback", "[rendering][uploadqueue]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    data::Mesh mesh;
    mesh.vertices.resize(3);

    bool called = false;
    MeshHandle resultHandle{};
    queue.enqueueMesh(mesh, [&](MeshResource resource) {
        called = true;
        resultHandle = resource.handle();
    });

    CHECK(queue.pendingCount() == 1);
    const std::size_t processed = queue.processQueue(renderer, 10);

    CHECK(processed == 1);
    CHECK(queue.pendingCount() == 0);
    CHECK(called);
    CHECK(resultHandle.isValid());
    CHECK(renderer.meshesCreated == 1);
    CHECK(renderer.meshVertexCounts.at(resultHandle.id) == 3);
}

TEST_CASE("GPUUploadQueue::processQueue respects the per-call cap", "[rendering][uploadqueue]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    for (int i = 0; i < 5; ++i)
    {
        queue.enqueueMesh(data::Mesh{}, [](MeshResource) {});
    }

    const std::size_t firstBatch = queue.processQueue(renderer, 2);
    CHECK(firstBatch == 2);
    CHECK(queue.pendingCount() == 3);

    const std::size_t secondBatch = queue.processQueue(renderer, 10);
    CHECK(secondBatch == 3);
    CHECK(queue.pendingCount() == 0);
    CHECK(renderer.meshesCreated == 5);
}

TEST_CASE("GPUUploadQueue uploads materials", "[rendering][uploadqueue]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    bool called = false;
    queue.enqueueMaterial(data::Material{}, [&](MaterialResource resource) {
        called = true;
        CHECK(resource.valid());
    });

    queue.processQueue(renderer, 10);
    CHECK(called);
    CHECK(renderer.materialsCreated == 1);
}

TEST_CASE("GPUUploadQueue processes queued meshes before materials within one call's budget", "[rendering][uploadqueue]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    queue.enqueueMesh(data::Mesh{}, [](MeshResource) {});
    queue.enqueueMaterial(data::Material{}, [](MaterialResource) {});

    const std::size_t processed = queue.processQueue(renderer, 1);
    CHECK(processed == 1);
    CHECK(renderer.meshesCreated == 1);
    CHECK(renderer.materialsCreated == 0);
    CHECK(queue.pendingCount() == 1);
}

TEST_CASE("A moved-from MeshResource callback result destroys nothing on its own", "[rendering][uploadqueue]")
{
    MockRenderer renderer;
    GPUUploadQueue queue;

    std::vector<MeshResource> kept;
    queue.enqueueMesh(data::Mesh{}, [&](MeshResource resource) { kept.push_back(std::move(resource)); });
    queue.processQueue(renderer, 10);

    REQUIRE(kept.size() == 1);
    CHECK(kept[0].valid());
    CHECK(renderer.destroyedMeshes.empty());

    kept.clear();
    CHECK(renderer.destroyedMeshes.size() == 1);
}
