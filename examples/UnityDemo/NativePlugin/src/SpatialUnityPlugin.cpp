#include "SpatialUnityPlugin.h"

#include <algorithm>
#include <cstring>

#include "spatial/SpatialWorld.h"

#include "ManagedMeshRenderer.h"

using namespace spatial;

namespace
{
    // Renderer must outlive world: world's GPU resources (as tracked by
    // ManagedMeshRenderer) reference ids it owns. Declaring renderer first
    // means it's destroyed after world — see docs/sdk_api.md's lifetime
    // rule, the same one StandaloneViewer and SpatialWorldTests follow.
    struct PluginState
    {
        unity::ManagedMeshRenderer renderer;
        SpatialWorld world;
    };

    PluginState& stateOf(SpatialUnityWorldHandle handle)
    {
        return *static_cast<PluginState*>(handle);
    }

    core::CameraParams toCameraParams(const SpatialUnityCameraParams& c)
    {
        core::CameraParams camera{};
        camera.position = core::Vec3{c.posX, c.posY, c.posZ};
        camera.forward = core::Vec3{c.fwdX, c.fwdY, c.fwdZ}.normalized();
        camera.verticalFovRadians = c.verticalFovRadians;
        camera.viewportHeightPx = c.viewportHeightPx;
        return camera;
    }

    SpatialUnityResult toResult(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::DatasetNotFound: return SpatialUnityResult_DatasetNotFound;
            case ErrorCode::InvalidDataset: return SpatialUnityResult_InvalidDataset;
            case ErrorCode::UnsupportedVersion: return SpatialUnityResult_UnsupportedVersion;
            case ErrorCode::TileLoadFailed: return SpatialUnityResult_TileLoadFailed;
            case ErrorCode::CorruptTile: return SpatialUnityResult_CorruptTile;
            case ErrorCode::OutOfMemory: return SpatialUnityResult_OutOfMemory;
            case ErrorCode::GPUUploadFailed: return SpatialUnityResult_GPUUploadFailed;
            case ErrorCode::InvalidState: return SpatialUnityResult_InvalidState;
        }
        return SpatialUnityResult_InvalidState;
    }
}

SpatialUnityWorldHandle SpatialUnity_CreateWorld()
{
    return new PluginState();
}

void SpatialUnity_DestroyWorld(SpatialUnityWorldHandle world)
{
    delete static_cast<PluginState*>(world);
}

SpatialUnityResult SpatialUnity_LoadDataset(SpatialUnityWorldHandle world, const char* manifestPath, SpatialUnityLoadConfig config)
{
    PluginState& state = stateOf(world);

    SpatialWorldConfig sdkConfig{};
    sdkConfig.streaming.streamingRadius = config.streamingRadius;
    sdkConfig.streaming.memoryBudget.maxResidentTiles = config.maxResidentTiles;
    sdkConfig.streaming.memoryBudget.cpuBudgetBytes = config.cpuMemoryBudgetBytes;
    sdkConfig.streaming.workerThreadCount = config.workerThreadCount;
    sdkConfig.maxGPUUploadsPerUpdate = config.maxGPUUploadsPerUpdate > 0 ? config.maxGPUUploadsPerUpdate : 8;
    sdkConfig.debugVisualizationEnabled = config.debugVisualizationEnabled != 0;

    const Expected<void> result = state.world.loadDataset(manifestPath, sdkConfig);
    return result.hasValue() ? SpatialUnityResult_Ok : toResult(result.error().code);
}

void SpatialUnity_Shutdown(SpatialUnityWorldHandle world)
{
    stateOf(world).world.shutdown();
}

int32_t SpatialUnity_IsLoaded(SpatialUnityWorldHandle world)
{
    return stateOf(world).world.isLoaded() ? 1 : 0;
}

std::uint32_t SpatialUnity_GetDatasetMaxLOD(SpatialUnityWorldHandle world)
{
    PluginState& state = stateOf(world);
    return state.world.isLoaded() ? state.world.datasetManifest().maxLOD : 0;
}

void SpatialUnity_SetDebugVisualization(SpatialUnityWorldHandle world, int32_t enabled)
{
    stateOf(world).world.setDebugVisualizationEnabled(enabled != 0);
}

int32_t SpatialUnity_GetDebugVisualization(SpatialUnityWorldHandle world)
{
    return stateOf(world).world.debugVisualizationEnabled() ? 1 : 0;
}

void SpatialUnity_Update(SpatialUnityWorldHandle world, SpatialUnityCameraParams camera)
{
    PluginState& state = stateOf(world);
    state.world.update(toCameraParams(camera), state.renderer);
}

void SpatialUnity_Render(SpatialUnityWorldHandle world, SpatialUnityCameraParams camera)
{
    PluginState& state = stateOf(world);
    const core::CameraParams cameraParams = toCameraParams(camera);

    // beginFrame()/endFrame() bracket world.render() the same way
    // StandaloneViewer's main loop brackets it around a real renderer;
    // beginFrame() is what clears ManagedMeshRenderer's per-frame draw
    // command / debug line lists.
    state.renderer.beginFrame(core::Mat4::identity());
    state.world.render(state.renderer, cameraParams);
    state.renderer.endFrame();
}

void SpatialUnity_GetStatistics(SpatialUnityWorldHandle world, SpatialUnityStatistics* outStats)
{
    if (outStats == nullptr)
    {
        return;
    }
    const streaming::StreamingStatistics stats = stateOf(world).world.statistics();
    outStats->requestedCount = stats.requestedCount;
    outStats->loadingCount = stats.loadingCount;
    outStats->residentCount = stats.residentCount;
    outStats->cpuMemoryUsedBytes = stats.cpuMemoryUsedBytes;
    outStats->gpuMemoryUsedBytes = stats.gpuMemoryUsedBytes;
    outStats->totalLoadsCompleted = stats.totalLoadsCompleted;
    outStats->totalLoadsFailed = stats.totalLoadsFailed;
    outStats->totalCancellations = stats.totalCancellations;
    outStats->totalUnloads = stats.totalUnloads;
    outStats->totalCacheHits = stats.totalCacheHits;
}

int32_t SpatialUnity_GetDrawCommandCount(SpatialUnityWorldHandle world)
{
    return static_cast<int32_t>(stateOf(world).renderer.drawCommands().size());
}

void SpatialUnity_GetDrawCommands(SpatialUnityWorldHandle world, int64_t* outMeshIds, int64_t* outMaterialIds, float* outTransforms16)
{
    const auto& commands = stateOf(world).renderer.drawCommands();
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto& cmd = commands[i];
        if (outMeshIds != nullptr)
        {
            outMeshIds[i] = static_cast<int64_t>(cmd.meshId);
        }
        if (outMaterialIds != nullptr)
        {
            outMaterialIds[i] = static_cast<int64_t>(cmd.materialId);
        }
        if (outTransforms16 != nullptr)
        {
            float* dst = outTransforms16 + (i * 16);
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    dst[(row * 4) + col] = cmd.worldTransform.m[row][col];
                }
            }
        }
    }
}

int32_t SpatialUnity_GetMeshVertexCount(SpatialUnityWorldHandle world, int64_t meshId)
{
    const auto* mesh = stateOf(world).renderer.findMesh(static_cast<std::uint64_t>(meshId));
    return mesh != nullptr ? static_cast<int32_t>(mesh->vertices.size()) : 0;
}

int32_t SpatialUnity_GetMeshIndexCount(SpatialUnityWorldHandle world, int64_t meshId)
{
    const auto* mesh = stateOf(world).renderer.findMesh(static_cast<std::uint64_t>(meshId));
    return mesh != nullptr ? static_cast<int32_t>(mesh->indices.size()) : 0;
}

int32_t SpatialUnity_GetMeshData(SpatialUnityWorldHandle world, int64_t meshId, float* outPositions3, float* outNormals3, float* outUVs2, int32_t* outIndices)
{
    const auto* mesh = stateOf(world).renderer.findMesh(static_cast<std::uint64_t>(meshId));
    if (mesh == nullptr)
    {
        return 0;
    }

    for (std::size_t i = 0; i < mesh->vertices.size(); ++i)
    {
        const data::Vertex& v = mesh->vertices[i];
        if (outPositions3 != nullptr)
        {
            outPositions3[(i * 3) + 0] = v.position.x;
            outPositions3[(i * 3) + 1] = v.position.y;
            outPositions3[(i * 3) + 2] = v.position.z;
        }
        if (outNormals3 != nullptr)
        {
            outNormals3[(i * 3) + 0] = v.normal.x;
            outNormals3[(i * 3) + 1] = v.normal.y;
            outNormals3[(i * 3) + 2] = v.normal.z;
        }
        if (outUVs2 != nullptr)
        {
            outUVs2[(i * 2) + 0] = v.uv.x;
            outUVs2[(i * 2) + 1] = v.uv.y;
        }
    }

    if (outIndices != nullptr)
    {
        for (std::size_t i = 0; i < mesh->indices.size(); ++i)
        {
            outIndices[i] = static_cast<int32_t>(mesh->indices[i]);
        }
    }

    return 1;
}

int32_t SpatialUnity_GetMaterialColor(SpatialUnityWorldHandle world, int64_t materialId, float* outRGBA4)
{
    const auto* material = stateOf(world).renderer.findMaterial(static_cast<std::uint64_t>(materialId));
    if (material == nullptr || outRGBA4 == nullptr)
    {
        return 0;
    }
    std::memcpy(outRGBA4, material->baseColor, sizeof(material->baseColor));
    return 1;
}

int32_t SpatialUnity_GetDebugLineVertexCount(SpatialUnityWorldHandle world)
{
    return static_cast<int32_t>(stateOf(world).renderer.debugLines().size());
}

void SpatialUnity_GetDebugLineData(SpatialUnityWorldHandle world, float* outPositions3, float* outColors4)
{
    const auto& lines = stateOf(world).renderer.debugLines();
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const rendering::DebugVertex& v = lines[i];
        if (outPositions3 != nullptr)
        {
            outPositions3[(i * 3) + 0] = v.position.x;
            outPositions3[(i * 3) + 1] = v.position.y;
            outPositions3[(i * 3) + 2] = v.position.z;
        }
        if (outColors4 != nullptr)
        {
            outColors4[(i * 4) + 0] = v.color.r;
            outColors4[(i * 4) + 1] = v.color.g;
            outColors4[(i * 4) + 2] = v.color.b;
            outColors4[(i * 4) + 3] = v.color.a;
        }
    }
}
