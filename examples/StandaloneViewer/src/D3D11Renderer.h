#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include "spatial/data/Material.h"
#include "spatial/rendering/IRenderer.h"

namespace viewer
{
    // The SDK's first real IRenderer backend. Direct3D 11, immediate
    // context only (no deferred contexts/command lists — not needed at
    // this scale). Owns the swap chain and is the only place in the viewer
    // that calls a graphics API directly; everything upstream of it only
    // ever talks to spatial::rendering::IRenderer.
    class D3D11Renderer final : public spatial::rendering::IRenderer
    {
    public:
        D3D11Renderer(HWND hwnd, int width, int height, const std::filesystem::path& shaderDir);
        ~D3D11Renderer() override = default;

        D3D11Renderer(const D3D11Renderer&) = delete;
        D3D11Renderer& operator=(const D3D11Renderer&) = delete;

        void resize(int width, int height);
        void clear(float r, float g, float b);
        void present();

        void beginFrame(const spatial::core::Mat4& viewProjection) override;
        void endFrame() override;

        [[nodiscard]] spatial::rendering::MeshHandle createMesh(const spatial::data::Mesh& mesh) override;
        void destroyMesh(spatial::rendering::MeshHandle handle) override;

        [[nodiscard]] spatial::rendering::MaterialHandle createMaterial(const spatial::data::Material& material) override;
        void destroyMaterial(spatial::rendering::MaterialHandle handle) override;

        [[nodiscard]] spatial::rendering::TextureHandle createTexture(std::span<const std::byte> pixels, std::uint32_t width, std::uint32_t height) override;
        void destroyTexture(spatial::rendering::TextureHandle handle) override;

        void drawMesh(spatial::rendering::MeshHandle mesh, spatial::rendering::MaterialHandle material, const spatial::core::Mat4& worldTransform) override;
        void drawDebugLines(std::span<const spatial::rendering::DebugVertex> vertices) override;

    private:
        struct MeshGPU
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
            UINT indexCount = 0;
        };

        void createDeviceAndSwapChain(HWND hwnd, int width, int height);
        void createRenderTargets(int width, int height);
        void loadShaders(const std::filesystem::path& shaderDir);
        void createPipelineState();

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;

        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_meshVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_meshPixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_meshInputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_meshConstantBuffer;

        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_lineVertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_linePixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_lineInputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_lineConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_lineVertexBuffer;
        UINT m_lineVertexBufferCapacity = 0;

        std::unordered_map<std::uint64_t, MeshGPU> m_meshes;
        std::unordered_map<std::uint64_t, spatial::data::Material> m_materials;
        std::unordered_map<std::uint64_t, Microsoft::WRL::ComPtr<ID3D11Texture2D>> m_textures;

        std::uint64_t m_nextMeshId = 0;
        std::uint64_t m_nextMaterialId = 0;
        std::uint64_t m_nextTextureId = 0;

        spatial::core::Mat4 m_viewProjection;
        int m_width = 0;
        int m_height = 0;
    };
}
