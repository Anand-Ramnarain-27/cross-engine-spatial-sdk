#include "ManagedMeshRenderer.h"

namespace spatial::examples
{
    void ManagedMeshRenderer::beginFrame(const core::Mat4& /*viewProjection*/)
    {
        m_drawCommands.clear();
        m_debugLines.clear();
    }

    void ManagedMeshRenderer::endFrame()
    {
    }

    rendering::MeshHandle ManagedMeshRenderer::createMesh(const data::Mesh& mesh)
    {
        const std::uint64_t id = m_nextId++;
        MeshData data{};
        data.vertices = mesh.vertices;
        data.indices = mesh.indices;
        m_meshes.emplace(id, std::move(data));
        return rendering::MeshHandle{id};
    }

    void ManagedMeshRenderer::destroyMesh(rendering::MeshHandle handle)
    {
        m_meshes.erase(handle.id);
    }

    rendering::MaterialHandle ManagedMeshRenderer::createMaterial(const data::Material& material)
    {
        const std::uint64_t id = m_nextId++;
        MaterialData data{};
        data.baseColor[0] = material.baseColorR;
        data.baseColor[1] = material.baseColorG;
        data.baseColor[2] = material.baseColorB;
        data.baseColor[3] = material.baseColorA;
        data.metallic = material.metallic;
        data.roughness = material.roughness;
        m_materials.emplace(id, data);
        return rendering::MaterialHandle{id};
    }

    void ManagedMeshRenderer::destroyMaterial(rendering::MaterialHandle handle)
    {
        m_materials.erase(handle.id);
    }

    rendering::TextureHandle ManagedMeshRenderer::createTexture(std::span<const std::byte> /*pixels*/, std::uint32_t width, std::uint32_t height)
    {
        const std::uint64_t id = m_nextId++;
        m_textureSizes.emplace(id, width * height);
        return rendering::TextureHandle{id};
    }

    void ManagedMeshRenderer::destroyTexture(rendering::TextureHandle handle)
    {
        m_textureSizes.erase(handle.id);
    }

    void ManagedMeshRenderer::drawMesh(rendering::MeshHandle mesh, rendering::MaterialHandle material, const core::Mat4& worldTransform)
    {
        m_drawCommands.push_back(DrawCommand{mesh.id, material.id, worldTransform});
    }

    void ManagedMeshRenderer::drawDebugLines(std::span<const rendering::DebugVertex> vertices)
    {
        m_debugLines.insert(m_debugLines.end(), vertices.begin(), vertices.end());
    }

    const ManagedMeshRenderer::MeshData* ManagedMeshRenderer::findMesh(std::uint64_t id) const
    {
        const auto it = m_meshes.find(id);
        return it != m_meshes.end() ? &it->second : nullptr;
    }

    const ManagedMeshRenderer::MaterialData* ManagedMeshRenderer::findMaterial(std::uint64_t id) const
    {
        const auto it = m_materials.find(id);
        return it != m_materials.end() ? &it->second : nullptr;
    }
}
