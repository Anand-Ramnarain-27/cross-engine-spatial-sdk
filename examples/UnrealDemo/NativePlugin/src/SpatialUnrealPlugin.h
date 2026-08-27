#pragma once

// Flat C ABI wrapping spatial::SpatialWorld for Unreal's plugin boundary.
// Structurally the same shape as examples/UnityDemo/NativePlugin's
// SpatialUnityPlugin.h (a separate, independently linkable binary — see
// docs/unreal_integration.md for why this isn't shared with Unity's plugin
// even though the C ABI approach and the IRenderer behind it are), with one
// real difference: every function here does its own right-handed<->Unreal
// coordinate conversion internally (see UnrealCoordinateConversion.h), so
// everything crossing this boundary is already Unreal-space (centimeters,
// left-handed, Z-up, correctly wound triangles). USpatialWorldComponent
// (the Unreal-side C++ class) does no conversion math of its own.
//
// Kept in sync by hand with
// UnrealDemo/SpatialSDKPlugin/Source/SpatialSDKPlugin/ — there's no codegen
// step, same as SpatialUnityPlugin.h/SpatialWorldNative.cs.

#include <cstdint>

#if defined(_WIN32)
    #define SPATIAL_UNREAL_API extern "C" __declspec(dllexport)
#else
    #define SPATIAL_UNREAL_API extern "C" __attribute__((visibility("default")))
#endif

extern "C"
{
    using SpatialUnrealWorldHandle = void*;

    // Matches spatial::ErrorCode's ordinal + 1; 0 means success. Keep in
    // sync with sdk/include/spatial/Error.h's ErrorCode enum order.
    enum SpatialUnrealResult : int32_t
    {
        SpatialUnrealResult_Ok = 0,
        SpatialUnrealResult_DatasetNotFound = 1,
        SpatialUnrealResult_InvalidDataset = 2,
        SpatialUnrealResult_UnsupportedVersion = 3,
        SpatialUnrealResult_TileLoadFailed = 4,
        SpatialUnrealResult_CorruptTile = 5,
        SpatialUnrealResult_OutOfMemory = 6,
        SpatialUnrealResult_GPUUploadFailed = 7,
        SpatialUnrealResult_InvalidState = 8,
    };

    // Position in centimeters, Unreal-space; forward need not be normalized.
    struct SpatialUnrealCameraParams
    {
        float posX, posY, posZ;
        float fwdX, fwdY, fwdZ;
        float verticalFovRadians;
        float viewportHeightPx;
    };

    struct SpatialUnrealLoadConfig
    {
        float streamingRadiusCm; // converted to meters internally
        std::uint32_t maxResidentTiles;
        std::uint64_t cpuMemoryBudgetBytes;
        std::uint32_t workerThreadCount;
        std::uint32_t maxGPUUploadsPerUpdate;
        int32_t debugVisualizationEnabled;
    };

    struct SpatialUnrealStatistics
    {
        std::uint64_t requestedCount;
        std::uint64_t loadingCount;
        std::uint64_t residentCount;
        std::uint64_t cpuMemoryUsedBytes;
        std::uint64_t gpuMemoryUsedBytes;
        std::uint64_t totalLoadsCompleted;
        std::uint64_t totalLoadsFailed;
        std::uint64_t totalCancellations;
        std::uint64_t totalUnloads;
        std::uint64_t totalCacheHits;
    };
}

SPATIAL_UNREAL_API SpatialUnrealWorldHandle SpatialUnreal_CreateWorld();
SPATIAL_UNREAL_API void SpatialUnreal_DestroyWorld(SpatialUnrealWorldHandle world);

SPATIAL_UNREAL_API SpatialUnrealResult SpatialUnreal_LoadDataset(SpatialUnrealWorldHandle world, const char* manifestPath, SpatialUnrealLoadConfig config);
SPATIAL_UNREAL_API void SpatialUnreal_Shutdown(SpatialUnrealWorldHandle world);
SPATIAL_UNREAL_API int32_t SpatialUnreal_IsLoaded(SpatialUnrealWorldHandle world);
SPATIAL_UNREAL_API std::uint32_t SpatialUnreal_GetDatasetMaxLOD(SpatialUnrealWorldHandle world);

SPATIAL_UNREAL_API void SpatialUnreal_SetDebugVisualization(SpatialUnrealWorldHandle world, int32_t enabled);
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetDebugVisualization(SpatialUnrealWorldHandle world);

SPATIAL_UNREAL_API void SpatialUnreal_Update(SpatialUnrealWorldHandle world, SpatialUnrealCameraParams camera);
// Records this frame's draw commands + debug lines, replacing whatever the
// previous call recorded. Pull them immediately afterward.
SPATIAL_UNREAL_API void SpatialUnreal_Render(SpatialUnrealWorldHandle world, SpatialUnrealCameraParams camera);

SPATIAL_UNREAL_API void SpatialUnreal_GetStatistics(SpatialUnrealWorldHandle world, SpatialUnrealStatistics* outStats);

// --- Per-frame draw commands ---
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetDrawCommandCount(SpatialUnrealWorldHandle world);
// outMeshIds/outMaterialIds: count int64 elements each. outTransforms16:
// count*16 floats, one row-major 4x4 (Unreal-space, already converted) per
// command.
SPATIAL_UNREAL_API void SpatialUnreal_GetDrawCommands(SpatialUnrealWorldHandle world, int64_t* outMeshIds, int64_t* outMaterialIds, float* outTransforms16);

// --- Mesh/material geometry, pulled once per id and cached Unreal-side ---
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetMeshVertexCount(SpatialUnrealWorldHandle world, int64_t meshId);
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetMeshIndexCount(SpatialUnrealWorldHandle world, int64_t meshId);
// outPositions3/outNormals3: vertexCount*3 floats, centimeters,
// Unreal-space. outUVs2: vertexCount*2 floats. outIndices: indexCount
// int32s, already rewound for Unreal's front-face convention. Returns 0 if
// meshId is unknown.
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetMeshData(SpatialUnrealWorldHandle world, int64_t meshId, float* outPositions3, float* outNormals3, float* outUVs2, int32_t* outIndices);

// outRGBA4: 4 floats. Returns 0 if materialId is unknown (0 = "no material").
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetMaterialColor(SpatialUnrealWorldHandle world, int64_t materialId, float* outRGBA4);

// --- Debug tile-bounds lines ---
SPATIAL_UNREAL_API int32_t SpatialUnreal_GetDebugLineVertexCount(SpatialUnrealWorldHandle world);
// outPositions3: count*3 floats, centimeters, Unreal-space. outColors4:
// count*4 floats. Consecutive pairs are one line segment.
SPATIAL_UNREAL_API void SpatialUnreal_GetDebugLineData(SpatialUnrealWorldHandle world, float* outPositions3, float* outColors4);
