#include "SpatialUnrealPlugin.h"

#include <cstring>

#include "spatial/SpatialWorld.h"

#include "ManagedMeshRenderer.h"
#include "UnrealCoordinateConversion.h"

using namespace spatial;
namespace conv = spatial::examples::unreal_convert;

namespace
{
    // renderer must outlive world — same lifetime rule as SpatialUnityPlugin
    // and documented throughout docs/sdk_api.md.
    struct PluginState
    {
        examples::ManagedMeshRenderer renderer;
        SpatialWorld world;
    };

    PluginState& stateOf(SpatialUnrealWorldHandle handle)
    {
        return *static_cast<PluginState*>(handle);
    }

    core::CameraParams toCameraParams(const SpatialUnrealCameraParams& c)
    {
        core::CameraParams camera{};
        camera.position = conv::toSpatialPosition(c.posX, c.posY, c.posZ);
        camera.forward = conv::toSpatialDirection(c.fwdX, c.fwdY, c.fwdZ).normalized();
        camera.verticalFovRadians = c.verticalFovRadians;
        camera.viewportHeightPx = c.viewportHeightPx;
        return camera;
    }

    SpatialUnrealResult toResult(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::DatasetNotFound: return SpatialUnrealResult_DatasetNotFound;
            case ErrorCode::InvalidDataset: return SpatialUnrealResult_InvalidDataset;
            case ErrorCode::UnsupportedVersion: return SpatialUnrealResult_UnsupportedVersion;
            case ErrorCode::TileLoadFailed: return SpatialUnrealResult_TileLoadFailed;
            case ErrorCode::CorruptTile: return SpatialUnrealResult_CorruptTile;
            case ErrorCode::OutOfMemory: return SpatialUnrealResult_OutOfMemory;
            case ErrorCode::GPUUploadFailed: return SpatialUnrealResult_GPUUploadFailed;
            case ErrorCode::InvalidState: return SpatialUnrealResult_InvalidState;
        }
        return SpatialUnrealResult_InvalidState;
    }
}

SpatialUnrealWorldHandle SpatialUnreal_CreateWorld()
{
    return new PluginState();
}

void SpatialUnreal_DestroyWorld(SpatialUnrealWorldHandle world)
{
    delete static_cast<PluginState*>(world);
}

SpatialUnrealResult SpatialUnreal_LoadDataset(SpatialUnrealWorldHandle world, const char* manifestPath, SpatialUnrealLoadConfig config)
{
    PluginState& state = stateOf(world);

    SpatialWorldConfig sdkConfig{};
    sdkConfig.streaming.streamingRadius = config.streamingRadiusCm / conv::kMetersToCentimeters;
    sdkConfig.streaming.memoryBudget.maxResidentTiles = config.maxResidentTiles;
    sdkConfig.streaming.memoryBudget.cpuBudgetBytes = config.cpuMemoryBudgetBytes;
    sdkConfig.streaming.workerThreadCount = config.workerThreadCount;
    sdkConfig.maxGPUUploadsPerUpdate = config.maxGPUUploadsPerUpdate > 0 ? config.maxGPUUploadsPerUpdate : 8;
    sdkConfig.debugVisualizationEnabled = config.debugVisualizationEnabled != 0;

    const Expected<void> result = state.world.loadDataset(manifestPath, sdkConfig);
    return result.hasValue() ? SpatialUnrealResult_Ok : toResult(result.error().code);
}

void SpatialUnreal_Shutdown(SpatialUnrealWorldHandle world)
{
    stateOf(world).world.shutdown();
}

int32_t SpatialUnreal_IsLoaded(SpatialUnrealWorldHandle world)
{
    return stateOf(world).world.isLoaded() ? 1 : 0;
}

std::uint32_t SpatialUnreal_GetDatasetMaxLOD(SpatialUnrealWorldHandle world)
{
    PluginState& state = stateOf(world);
    return state.world.isLoaded() ? state.world.datasetManifest().maxLOD : 0;
}

void SpatialUnreal_SetDebugVisualization(SpatialUnrealWorldHandle world, int32_t enabled)
{
    stateOf(world).world.setDebugVisualizationEnabled(enabled != 0);
}

int32_t SpatialUnreal_GetDebugVisualization(SpatialUnrealWorldHandle world)
{
    return stateOf(world).world.debugVisualizationEnabled() ? 1 : 0;
}

void SpatialUnreal_Update(SpatialUnrealWorldHandle world, SpatialUnrealCameraParams camera)
{
    PluginState& state = stateOf(world);
    state.world.update(toCameraParams(camera), state.renderer);
}

void SpatialUnreal_Render(SpatialUnrealWorldHandle world, SpatialUnrealCameraParams camera)
{
    PluginState& state = stateOf(world);
    const core::CameraParams cameraParams = toCameraParams(camera);

    state.renderer.beginFrame(core::Mat4::identity());
    state.world.render(state.renderer, cameraParams);
    state.renderer.endFrame();
}

void SpatialUnreal_GetStatistics(SpatialUnrealWorldHandle world, SpatialUnrealStatistics* outStats)
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

int32_t SpatialUnreal_GetDrawCommandCount(SpatialUnrealWorldHandle world)
{
    return static_cast<int32_t>(stateOf(world).renderer.drawCommands().size());
}

void SpatialUnreal_GetDrawCommands(SpatialUnrealWorldHandle world, int64_t* outMeshIds, int64_t* outMaterialIds, float* outTransforms16)
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
            const core::Mat4 ueTransform = conv::transform(cmd.worldTransform);
            float* dst = outTransforms16 + (i * 16);
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    dst[(row * 4) + col] = ueTransform.m[row][col];
                }
            }
        }
    }
}

int32_t SpatialUnreal_GetMeshVertexCount(SpatialUnrealWorldHandle world, int64_t meshId)
{
    const auto* mesh = stateOf(world).renderer.findMesh(static_cast<std::uint64_t>(meshId));
    return mesh != nullptr ? static_cast<int32_t>(mesh->vertices.size()) : 0;
}

int32_t SpatialUnreal_GetMeshIndexCount(SpatialUnrealWorldHandle world, int64_t meshId)
{
    const auto* mesh = stateOf(world).renderer.findMesh(static_cast<std::uint64_t>(meshId));
    return mesh != nullptr ? static_cast<int32_t>(mesh->indices.size()) : 0;
}

int32_t SpatialUnreal_GetMeshData(SpatialUnrealWorldHandle world, int64_t meshId, float* outPositions3, float* outNormals3, float* outUVs2, int32_t* outIndices)
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
            conv::position(v.position.x, v.position.y, v.position.z, outPositions3[(i * 3) + 0], outPositions3[(i * 3) + 1], outPositions3[(i * 3) + 2]);
        }
        if (outNormals3 != nullptr)
        {
            conv::direction(v.normal.x, v.normal.y, v.normal.z, outNormals3[(i * 3) + 0], outNormals3[(i * 3) + 1], outNormals3[(i * 3) + 2]);
        }
        if (outUVs2 != nullptr)
        {
            outUVs2[(i * 2) + 0] = v.uv.x;
            outUVs2[(i * 2) + 1] = v.uv.y;
        }
    }

    // Reversing handedness (the axis swap in conv::position/direction)
    // flips front-face winding, so each triangle's last two indices are
    // swapped here to restore Unreal's expected front-face winding — same
    // fix as examples/UnityDemo's mesh building, done natively instead.
    if (outIndices != nullptr)
    {
        for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3)
        {
            outIndices[i + 0] = static_cast<int32_t>(mesh->indices[i + 0]);
            outIndices[i + 1] = static_cast<int32_t>(mesh->indices[i + 2]);
            outIndices[i + 2] = static_cast<int32_t>(mesh->indices[i + 1]);
        }
    }

    return 1;
}

int32_t SpatialUnreal_GetMaterialColor(SpatialUnrealWorldHandle world, int64_t materialId, float* outRGBA4)
{
    const auto* material = stateOf(world).renderer.findMaterial(static_cast<std::uint64_t>(materialId));
    if (material == nullptr || outRGBA4 == nullptr)
    {
        return 0;
    }
    std::memcpy(outRGBA4, material->baseColor, sizeof(material->baseColor));
    return 1;
}

int32_t SpatialUnreal_GetDebugLineVertexCount(SpatialUnrealWorldHandle world)
{
    return static_cast<int32_t>(stateOf(world).renderer.debugLines().size());
}

void SpatialUnreal_GetDebugLineData(SpatialUnrealWorldHandle world, float* outPositions3, float* outColors4)
{
    const auto& lines = stateOf(world).renderer.debugLines();
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const rendering::DebugVertex& v = lines[i];
        if (outPositions3 != nullptr)
        {
            conv::position(v.position.x, v.position.y, v.position.z, outPositions3[(i * 3) + 0], outPositions3[(i * 3) + 1], outPositions3[(i * 3) + 2]);
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
