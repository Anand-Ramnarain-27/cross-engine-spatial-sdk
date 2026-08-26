#pragma once

#include "spatial/rendering/IRenderer.h"
#include "spatial/rendering/ResourceHandle.h"

namespace spatial::rendering
{
    // RAII wrapper around a GPU handle: calls `Destroy` on the owning
    // IRenderer when the wrapper is destroyed, moved-from, or reset().
    // Holds a non-owning IRenderer pointer — the renderer must outlive
    // every resource it created.
    template <typename HandleT, void (IRenderer::*Destroy)(HandleT)>
    class GPUResource
    {
    public:
        GPUResource() = default;
        GPUResource(IRenderer& renderer, HandleT handle) noexcept : m_renderer(&renderer), m_handle(handle) {}
        ~GPUResource() { reset(); }

        GPUResource(const GPUResource&) = delete;
        GPUResource& operator=(const GPUResource&) = delete;

        GPUResource(GPUResource&& other) noexcept : m_renderer(other.m_renderer), m_handle(other.m_handle)
        {
            other.m_renderer = nullptr;
            other.m_handle = HandleT{};
        }

        GPUResource& operator=(GPUResource&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                m_renderer = other.m_renderer;
                m_handle = other.m_handle;
                other.m_renderer = nullptr;
                other.m_handle = HandleT{};
            }
            return *this;
        }

        [[nodiscard]] HandleT handle() const noexcept { return m_handle; }
        [[nodiscard]] bool valid() const noexcept { return m_handle.isValid(); }

        void reset()
        {
            if (m_renderer != nullptr && m_handle.isValid())
            {
                (m_renderer->*Destroy)(m_handle);
            }
            m_renderer = nullptr;
            m_handle = HandleT{};
        }

    private:
        IRenderer* m_renderer = nullptr;
        HandleT m_handle{};
    };

    using MeshResource = GPUResource<MeshHandle, &IRenderer::destroyMesh>;
    using MaterialResource = GPUResource<MaterialHandle, &IRenderer::destroyMaterial>;
    using TextureResource = GPUResource<TextureHandle, &IRenderer::destroyTexture>;
}
