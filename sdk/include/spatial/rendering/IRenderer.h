#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "spatial/core/Mat4.h"
#include "spatial/data/Material.h"
#include "spatial/data/Mesh.h"
#include "spatial/rendering/DebugVertex.h"
#include "spatial/rendering/ResourceHandle.h"

namespace spatial::rendering
{
    // Engine-agnostic GPU boundary: Spatial SDK code only ever calls
    // through this interface, never a concrete graphics API. A backend
    // (custom renderer, Unity, Unreal) implements it. Deliberately minimal
    // — no render passes or pipeline state, just enough surface to
    // create/destroy/upload what the SDK produces and submit it for drawing.
    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual void beginFrame(const core::Mat4& viewProjection) = 0;
        virtual void endFrame() = 0;

        [[nodiscard]] virtual MeshHandle createMesh(const data::Mesh& mesh) = 0;
        virtual void destroyMesh(MeshHandle handle) = 0;

        [[nodiscard]] virtual MaterialHandle createMaterial(const data::Material& material) = 0;
        virtual void destroyMaterial(MaterialHandle handle) = 0;

        // `pixels` is tightly-packed RGBA8: width * height * 4 bytes.
        [[nodiscard]] virtual TextureHandle createTexture(std::span<const std::byte> pixels, std::uint32_t width, std::uint32_t height) = 0;
        virtual void destroyTexture(TextureHandle handle) = 0;

        virtual void drawMesh(MeshHandle mesh, MaterialHandle material, const core::Mat4& worldTransform) = 0;

        // Line list: consecutive vertex pairs are one segment.
        virtual void drawDebugLines(std::span<const DebugVertex> vertices) = 0;
    };
}
