#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "spatial/rendering/IRenderer.h"

namespace spatial::examples
{
    // IRenderer shared by every "managed mesh bridge" native plugin
    // (currently SpatialUnityPlugin and SpatialUnrealPlugin). Unlike
    // D3D11Renderer, this implementation never touches a graphics API:
    // create*/destroy* just manage CPU-side storage keyed by a fresh id,
    // and drawMesh/drawDebugLines record what was submitted for the
    // current frame. Each engine's plugin exposes that state across its
    // own flat C API; the engine-side code pulls it and turns it into
    // real engine geometry (UnityEngine.Mesh + Graphics.DrawMesh,
    // UProceduralMeshComponent, etc). Genuinely engine-agnostic — nothing
    // here references Unity or Unreal — which is why it lives in
    // examples/common rather than under either engine's demo folder. See
    // docs/unity_integration.md / docs/unreal_integration.md for the
    // per-engine rationale.
    class ManagedMeshRenderer final : public rendering::IRenderer
    {
    public:
        struct MeshData
        {
            std::vector<data::Vertex> vertices;
            std::vector<std::uint32_t> indices;
        };

        struct MaterialData
        {
            float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            float metallic = 0.0f;
            float roughness = 1.0f;
        };

        // worldTransform is recorded even though SpatialWorld::render() only
        // ever passes identity today — this is a faithful passthrough of
        // whatever IRenderer::drawMesh receives, not a guarantee callers
        // should rely on.
        struct DrawCommand
        {
            std::uint64_t meshId = 0;
            std::uint64_t materialId = 0; // 0 = none
            core::Mat4 worldTransform;
        };

        void beginFrame(const core::Mat4& viewProjection) override;
        void endFrame() override;

        [[nodiscard]] rendering::MeshHandle createMesh(const data::Mesh& mesh) override;
        void destroyMesh(rendering::MeshHandle handle) override;

        [[nodiscard]] rendering::MaterialHandle createMaterial(const data::Material& material) override;
        void destroyMaterial(rendering::MaterialHandle handle) override;

        [[nodiscard]] rendering::TextureHandle createTexture(std::span<const std::byte> pixels, std::uint32_t width, std::uint32_t height) override;
        void destroyTexture(rendering::TextureHandle handle) override;

        void drawMesh(rendering::MeshHandle mesh, rendering::MaterialHandle material, const core::Mat4& worldTransform) override;
        void drawDebugLines(std::span<const rendering::DebugVertex> vertices) override;

        // Pull API used by SpatialUnityPlugin.cpp. Mesh/material data is
        // looked up once per id (cached C# side); draw commands and debug
        // lines are only valid for the frame just rendered, since
        // beginFrame() clears both.
        [[nodiscard]] const MeshData* findMesh(std::uint64_t id) const;
        [[nodiscard]] const MaterialData* findMaterial(std::uint64_t id) const;
        [[nodiscard]] const std::vector<DrawCommand>& drawCommands() const noexcept { return m_drawCommands; }
        [[nodiscard]] const std::vector<rendering::DebugVertex>& debugLines() const noexcept { return m_debugLines; }

        [[nodiscard]] std::size_t meshCount() const noexcept { return m_meshes.size(); }
        [[nodiscard]] std::size_t materialCount() const noexcept { return m_materials.size(); }

    private:
        std::uint64_t m_nextId = 1;
        std::unordered_map<std::uint64_t, MeshData> m_meshes;
        std::unordered_map<std::uint64_t, MaterialData> m_materials;
        std::unordered_map<std::uint64_t, std::uint32_t> m_textureSizes; // id -> width*height, unused today
        std::vector<DrawCommand> m_drawCommands;
        std::vector<rendering::DebugVertex> m_debugLines;
    };
}
