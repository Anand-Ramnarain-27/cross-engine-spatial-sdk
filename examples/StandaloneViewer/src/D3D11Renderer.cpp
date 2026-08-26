#include "D3D11Renderer.h"

#include <d3dcompiler.h>

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "spatial/core/Mat4.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using spatial::core::Mat4;
using spatial::rendering::MaterialHandle;
using spatial::rendering::MeshHandle;
using spatial::rendering::TextureHandle;

namespace viewer
{
    namespace
    {
        void throwIfFailed(HRESULT hr, const char* what)
        {
            if (FAILED(hr))
            {
                throw std::runtime_error(std::string(what) + " failed (HRESULT " + std::to_string(hr) + ")");
            }
        }

        std::string readFile(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open())
            {
                throw std::runtime_error("Could not open shader file: " + path.string());
            }
            std::ostringstream buffer;
            buffer << in.rdbuf();
            return buffer.str();
        }

        ComPtr<ID3DBlob> compileShader(const std::filesystem::path& path, const char* entryPoint, const char* target)
        {
            const std::string source = readFile(path);

            ComPtr<ID3DBlob> code;
            ComPtr<ID3DBlob> errors;
            const HRESULT hr = D3DCompile(
                source.data(), source.size(), path.string().c_str(), nullptr, nullptr,
                entryPoint, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);

            if (FAILED(hr))
            {
                std::string message = "Shader compile failed: " + path.string();
                if (errors)
                {
                    message += "\n";
                    message.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
                }
                throw std::runtime_error(message);
            }
            return code;
        }

        // Struct layouts must match the HLSL cbuffers exactly (row_major
        // matrix, same field order, 16-byte-multiple total size).
        struct PerDrawConstants
        {
            Mat4 worldViewProj;
            float baseColor[4];
        };

        struct PerFrameConstants
        {
            Mat4 viewProj;
        };
    }

    D3D11Renderer::D3D11Renderer(HWND hwnd, int width, int height, const std::filesystem::path& shaderDir)
        : m_width(width), m_height(height)
    {
        createDeviceAndSwapChain(hwnd, width, height);
        createRenderTargets(width, height);
        loadShaders(shaderDir);
        createPipelineState();
    }

    void D3D11Renderer::createDeviceAndSwapChain(HWND hwnd, int width, int height)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = static_cast<UINT>(width);
        desc.BufferDesc.Height = static_cast<UINT>(height);
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.OutputWindow = hwnd;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL obtainedLevel{};

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            &requestedLevel, 1, D3D11_SDK_VERSION, &desc,
            &m_swapChain, &m_device, &obtainedLevel, &m_context);
        throwIfFailed(hr, "D3D11CreateDeviceAndSwapChain");
    }

    void D3D11Renderer::createRenderTargets(int width, int height)
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        throwIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "IDXGISwapChain::GetBuffer");
        throwIfFailed(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView), "CreateRenderTargetView");

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = static_cast<UINT>(width);
        depthDesc.Height = static_cast<UINT>(height);
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        throwIfFailed(m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencilBuffer), "CreateTexture2D (depth)");
        throwIfFailed(m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, &m_depthStencilView), "CreateDepthStencilView");

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);
    }

    void D3D11Renderer::loadShaders(const std::filesystem::path& shaderDir)
    {
        {
            const ComPtr<ID3DBlob> vsCode = compileShader(shaderDir / "Mesh.hlsl", "VSMain", "vs_5_0");
            const ComPtr<ID3DBlob> psCode = compileShader(shaderDir / "Mesh.hlsl", "PSMain", "ps_5_0");
            throwIfFailed(m_device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_meshVertexShader), "CreateVertexShader (Mesh)");
            throwIfFailed(m_device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_meshPixelShader), "CreatePixelShader (Mesh)");

            const std::array<D3D11_INPUT_ELEMENT_DESC, 3> layout = {{
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            }};
            throwIfFailed(m_device->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &m_meshInputLayout), "CreateInputLayout (Mesh)");
        }
        {
            const ComPtr<ID3DBlob> vsCode = compileShader(shaderDir / "DebugLine.hlsl", "VSMain", "vs_5_0");
            const ComPtr<ID3DBlob> psCode = compileShader(shaderDir / "DebugLine.hlsl", "PSMain", "ps_5_0");
            throwIfFailed(m_device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_lineVertexShader), "CreateVertexShader (DebugLine)");
            throwIfFailed(m_device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_linePixelShader), "CreatePixelShader (DebugLine)");

            const std::array<D3D11_INPUT_ELEMENT_DESC, 2> layout = {{
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            }};
            throwIfFailed(m_device->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &m_lineInputLayout), "CreateInputLayout (DebugLine)");
        }
    }

    void D3D11Renderer::createPipelineState()
    {
        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        cbDesc.ByteWidth = sizeof(PerDrawConstants);
        throwIfFailed(m_device->CreateBuffer(&cbDesc, nullptr, &m_meshConstantBuffer), "CreateBuffer (PerDraw)");

        cbDesc.ByteWidth = sizeof(PerFrameConstants);
        throwIfFailed(m_device->CreateBuffer(&cbDesc, nullptr, &m_lineConstantBuffer), "CreateBuffer (PerFrame)");

        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
        throwIfFailed(m_device->CreateDepthStencilState(&depthDesc, &m_depthStencilState), "CreateDepthStencilState");

        // No culling: tile geometry winding isn't guaranteed consistent
        // (procedural generation, and later glTF/OBJ import), and getting
        // it wrong would silently make geometry disappear. Correctness
        // over the minor overdraw cost at this scale.
        D3D11_RASTERIZER_DESC rasterDesc{};
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = D3D11_CULL_NONE;
        rasterDesc.DepthClipEnable = TRUE;
        throwIfFailed(m_device->CreateRasterizerState(&rasterDesc, &m_rasterizerState), "CreateRasterizerState");
    }

    void D3D11Renderer::resize(int width, int height)
    {
        if (width <= 0 || height <= 0 || (width == m_width && height == m_height))
        {
            return;
        }

        m_renderTargetView.Reset();
        m_depthStencilView.Reset();
        m_depthStencilBuffer.Reset();

        throwIfFailed(m_swapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");

        m_width = width;
        m_height = height;
        createRenderTargets(width, height);
    }

    void D3D11Renderer::clear(float r, float g, float b)
    {
        const float color[4] = {r, g, b, 1.0f};
        m_context->ClearRenderTargetView(m_renderTargetView.Get(), color);
        m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    void D3D11Renderer::present() { m_swapChain->Present(1, 0); }

    void D3D11Renderer::beginFrame(const Mat4& viewProjection)
    {
        m_viewProjection = viewProjection;

        ID3D11RenderTargetView* rtv = m_renderTargetView.Get();
        m_context->OMSetRenderTargets(1, &rtv, m_depthStencilView.Get());
        m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
        m_context->RSSetState(m_rasterizerState.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    void D3D11Renderer::endFrame()
    {
        // Nothing to flush explicitly with an immediate context; present()
        // is called separately so the caller can draw debug overlays after
        // world geometry without a second beginFrame/endFrame pair.
    }

    MeshHandle D3D11Renderer::createMesh(const spatial::data::Mesh& mesh)
    {
        MeshGPU gpu;
        gpu.indexCount = static_cast<UINT>(mesh.indices.size());

        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(spatial::data::Vertex));
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = mesh.vertices.data();
        throwIfFailed(m_device->CreateBuffer(&vbDesc, &vbData, &gpu.vertexBuffer), "CreateBuffer (vertex)");

        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(std::uint32_t));
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = mesh.indices.data();
        throwIfFailed(m_device->CreateBuffer(&ibDesc, &ibData, &gpu.indexBuffer), "CreateBuffer (index)");

        const MeshHandle handle{++m_nextMeshId};
        m_meshes.emplace(handle.id, std::move(gpu));
        return handle;
    }

    void D3D11Renderer::destroyMesh(MeshHandle handle) { m_meshes.erase(handle.id); }

    MaterialHandle D3D11Renderer::createMaterial(const spatial::data::Material& material)
    {
        const MaterialHandle handle{++m_nextMaterialId};
        m_materials.emplace(handle.id, material);
        return handle;
    }

    void D3D11Renderer::destroyMaterial(MaterialHandle handle) { m_materials.erase(handle.id); }

    TextureHandle D3D11Renderer::createTexture(std::span<const std::byte> pixels, std::uint32_t width, std::uint32_t height)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData{};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = width * 4;

        ComPtr<ID3D11Texture2D> texture;
        throwIfFailed(m_device->CreateTexture2D(&desc, pixels.empty() ? nullptr : &initData, &texture), "CreateTexture2D");

        const TextureHandle handle{++m_nextTextureId};
        m_textures.emplace(handle.id, std::move(texture));
        return handle;
    }

    void D3D11Renderer::destroyTexture(TextureHandle handle) { m_textures.erase(handle.id); }

    void D3D11Renderer::drawMesh(MeshHandle mesh, MaterialHandle material, const Mat4& worldTransform)
    {
        const auto meshIt = m_meshes.find(mesh.id);
        if (meshIt == m_meshes.end())
        {
            return;
        }

        PerDrawConstants constants{};
        constants.worldViewProj = m_viewProjection * worldTransform;

        const auto materialIt = m_materials.find(material.id);
        if (materialIt != m_materials.end())
        {
            constants.baseColor[0] = materialIt->second.baseColorR;
            constants.baseColor[1] = materialIt->second.baseColorG;
            constants.baseColor[2] = materialIt->second.baseColorB;
            constants.baseColor[3] = materialIt->second.baseColorA;
        }
        else
        {
            constants.baseColor[0] = constants.baseColor[1] = constants.baseColor[2] = constants.baseColor[3] = 1.0f;
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        m_context->Map(m_meshConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        std::memcpy(mapped.pData, &constants, sizeof(constants));
        m_context->Unmap(m_meshConstantBuffer.Get(), 0);

        const MeshGPU& gpu = meshIt->second;
        const UINT stride = sizeof(spatial::data::Vertex);
        const UINT offset = 0;

        m_context->IASetInputLayout(m_meshInputLayout.Get());
        m_context->IASetVertexBuffers(0, 1, gpu.vertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetIndexBuffer(gpu.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_context->VSSetShader(m_meshVertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_meshPixelShader.Get(), nullptr, 0);
        ID3D11Buffer* cb = m_meshConstantBuffer.Get();
        m_context->VSSetConstantBuffers(0, 1, &cb);
        m_context->PSSetConstantBuffers(0, 1, &cb);

        m_context->DrawIndexed(gpu.indexCount, 0, 0);
    }

    void D3D11Renderer::drawDebugLines(std::span<const spatial::rendering::DebugVertex> vertices)
    {
        if (vertices.empty())
        {
            return;
        }

        if (vertices.size() > m_lineVertexBufferCapacity)
        {
            m_lineVertexBufferCapacity = static_cast<UINT>(vertices.size());
            D3D11_BUFFER_DESC vbDesc{};
            vbDesc.ByteWidth = m_lineVertexBufferCapacity * sizeof(spatial::rendering::DebugVertex);
            vbDesc.Usage = D3D11_USAGE_DYNAMIC;
            vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            throwIfFailed(m_device->CreateBuffer(&vbDesc, nullptr, &m_lineVertexBuffer), "CreateBuffer (debug line vertex)");
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        m_context->Map(m_lineVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(spatial::rendering::DebugVertex));
        m_context->Unmap(m_lineVertexBuffer.Get(), 0);

        PerFrameConstants constants{m_viewProjection};
        D3D11_MAPPED_SUBRESOURCE mappedCb{};
        m_context->Map(m_lineConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCb);
        std::memcpy(mappedCb.pData, &constants, sizeof(constants));
        m_context->Unmap(m_lineConstantBuffer.Get(), 0);

        const UINT stride = sizeof(spatial::rendering::DebugVertex);
        const UINT offset = 0;

        m_context->IASetInputLayout(m_lineInputLayout.Get());
        m_context->IASetVertexBuffers(0, 1, m_lineVertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_context->VSSetShader(m_lineVertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_linePixelShader.Get(), nullptr, 0);
        ID3D11Buffer* cb = m_lineConstantBuffer.Get();
        m_context->VSSetConstantBuffers(0, 1, &cb);

        m_context->Draw(static_cast<UINT>(vertices.size()), 0);

        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }
}
