#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "spatial/rendering/GPUResource.h"

#include "rendering/MockRenderer.h"

using namespace spatial;
using namespace spatial::rendering;
using spatial::tests::MockRenderer;

TEST_CASE("MeshResource destroys its handle when it goes out of scope", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    const MeshHandle handle = renderer.createMesh(data::Mesh{});
    {
        const MeshResource resource(renderer, handle);
        CHECK(resource.valid());
        CHECK(resource.handle() == handle);
        CHECK(renderer.destroyedMeshes.empty());
    }
    REQUIRE(renderer.destroyedMeshes.size() == 1);
    CHECK(renderer.destroyedMeshes[0] == handle);
}

TEST_CASE("MeshResource move transfers ownership without double-destroying", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    const MeshHandle handle = renderer.createMesh(data::Mesh{});
    {
        MeshResource a(renderer, handle);
        MeshResource b = std::move(a);

        CHECK_FALSE(a.valid());
        CHECK(b.valid());
        CHECK(renderer.destroyedMeshes.empty());
    }
    CHECK(renderer.destroyedMeshes.size() == 1);
}

TEST_CASE("MeshResource move-assignment destroys the old handle first", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    const MeshHandle handleA = renderer.createMesh(data::Mesh{});
    const MeshHandle handleB = renderer.createMesh(data::Mesh{});

    MeshResource a(renderer, handleA);
    MeshResource b(renderer, handleB);
    a = std::move(b);

    REQUIRE(renderer.destroyedMeshes.size() == 1);
    CHECK(renderer.destroyedMeshes[0] == handleA); // a's original handle was released
    CHECK(a.handle() == handleB);
}

TEST_CASE("MeshResource::reset destroys immediately and is idempotent", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    const MeshHandle handle = renderer.createMesh(data::Mesh{});
    MeshResource resource(renderer, handle);

    resource.reset();
    CHECK_FALSE(resource.valid());
    CHECK(renderer.destroyedMeshes.size() == 1);

    resource.reset(); // must not double-destroy
    CHECK(renderer.destroyedMeshes.size() == 1);
}

TEST_CASE("Default-constructed MeshResource destroys nothing", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    {
        const MeshResource resource;
        CHECK_FALSE(resource.valid());
    }
    CHECK(renderer.destroyedMeshes.empty());
}

TEST_CASE("MaterialResource and TextureResource follow the same RAII pattern", "[rendering][gpuresource]")
{
    MockRenderer renderer;
    const MaterialHandle materialHandle = renderer.createMaterial(data::Material{});
    const TextureHandle textureHandle = renderer.createTexture({}, 1, 1);

    {
        const MaterialResource material(renderer, materialHandle);
        const TextureResource texture(renderer, textureHandle);
        CHECK(material.valid());
        CHECK(texture.valid());
    }

    CHECK(renderer.destroyedMaterials == std::vector<MaterialHandle>{materialHandle});
    CHECK(renderer.destroyedTextures == std::vector<TextureHandle>{textureHandle});
}
