#pragma once

// Test-only IRenderer implementation: records every call instead of talking
// to a real GPU, so rendering-layer behavior (RAII, upload batching, debug
// draw batching) can be verified deterministically without a window/context.

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "spatial/rendering/IRenderer.h"

namespace spatial::tests
{
    class MockRenderer final : public rendering::IRenderer
    {
    public:
        struct DrawMeshCall
        {
            rendering::MeshHandle mesh;
            rendering::MaterialHandle material;
            core::Mat4 transform;
        };

        void beginFrame(const core::Mat4& viewProjection) override
        {
            ++beginFrameCount;
            lastViewProjection = viewProjection;
        }

        void endFrame() override { ++endFrameCount; }

        rendering::MeshHandle createMesh(const data::Mesh& mesh) override
        {
            const rendering::MeshHandle handle{++m_nextMeshId};
            meshVertexCounts[handle.id] = mesh.vertices.size();
            ++meshesCreated;
            return handle;
        }

        void destroyMesh(rendering::MeshHandle handle) override { destroyedMeshes.push_back(handle); }

        rendering::MaterialHandle createMaterial(const data::Material&) override
        {
            const rendering::MaterialHandle handle{++m_nextMaterialId};
            ++materialsCreated;
            return handle;
        }

        void destroyMaterial(rendering::MaterialHandle handle) override { destroyedMaterials.push_back(handle); }

        rendering::TextureHandle createTexture(std::span<const std::byte>, std::uint32_t, std::uint32_t) override
        {
            const rendering::TextureHandle handle{++m_nextTextureId};
            ++texturesCreated;
            return handle;
        }

        void destroyTexture(rendering::TextureHandle handle) override { destroyedTextures.push_back(handle); }

        void drawMesh(rendering::MeshHandle mesh, rendering::MaterialHandle material, const core::Mat4& transform) override
        {
            drawMeshCalls.push_back(DrawMeshCall{mesh, material, transform});
        }

        void drawDebugLines(std::span<const rendering::DebugVertex> vertices) override
        {
            debugLineVertices.insert(debugLineVertices.end(), vertices.begin(), vertices.end());
            ++debugLineBatchCount;
        }

        int beginFrameCount = 0;
        int endFrameCount = 0;
        core::Mat4 lastViewProjection;

        int meshesCreated = 0;
        int materialsCreated = 0;
        int texturesCreated = 0;
        std::unordered_map<std::uint64_t, std::size_t> meshVertexCounts;

        std::vector<rendering::MeshHandle> destroyedMeshes;
        std::vector<rendering::MaterialHandle> destroyedMaterials;
        std::vector<rendering::TextureHandle> destroyedTextures;

        std::vector<DrawMeshCall> drawMeshCalls;
        std::vector<rendering::DebugVertex> debugLineVertices;
        int debugLineBatchCount = 0;

    private:
        std::uint64_t m_nextMeshId = 0;
        std::uint64_t m_nextMaterialId = 0;
        std::uint64_t m_nextTextureId = 0;
    };
}
