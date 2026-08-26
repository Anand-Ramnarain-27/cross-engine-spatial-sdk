#pragma once

// Flat C ABI wrapping spatial::SpatialWorld for Unity's P/Invoke marshaler —
// C# cannot call C++ member functions directly, so every operation is a
// free function taking an opaque handle. Only fixed-layout structs and
// primitive types cross this boundary. Mirrored by
// UnityProject/Assets/SpatialSDK/Scripts/Native/SpatialWorldNative.cs, whose
// [DllImport] declarations and [StructLayout(LayoutKind.Sequential)] structs
// must stay in sync with the definitions below field-for-field.

#include <cstdint>

#if defined(_WIN32)
    #define SPATIAL_UNITY_API extern "C" __declspec(dllexport)
#else
    #define SPATIAL_UNITY_API extern "C" __attribute__((visibility("default")))
#endif

extern "C"
{
    using SpatialUnityWorldHandle = void*;

    // Matches spatial::ErrorCode's ordinal + 1; 0 means success. Keep in
    // sync with sdk/include/spatial/Error.h's ErrorCode enum order.
    enum SpatialUnityResult : int32_t
    {
        SpatialUnityResult_Ok = 0,
        SpatialUnityResult_DatasetNotFound = 1,
        SpatialUnityResult_InvalidDataset = 2,
        SpatialUnityResult_UnsupportedVersion = 3,
        SpatialUnityResult_TileLoadFailed = 4,
        SpatialUnityResult_CorruptTile = 5,
        SpatialUnityResult_OutOfMemory = 6,
        SpatialUnityResult_GPUUploadFailed = 7,
        SpatialUnityResult_InvalidState = 8,
    };

    struct SpatialUnityCameraParams
    {
        float posX, posY, posZ;
        float fwdX, fwdY, fwdZ; // need not be normalized on input; normalized before use
        float verticalFovRadians;
        float viewportHeightPx;
    };

    struct SpatialUnityLoadConfig
    {
        float streamingRadius;
        std::uint32_t maxResidentTiles;
        std::uint64_t cpuMemoryBudgetBytes;
        std::uint32_t workerThreadCount;
        std::uint32_t maxGPUUploadsPerUpdate;
        int32_t debugVisualizationEnabled; // bool as int32 — blittable for C#
    };

    struct SpatialUnityStatistics
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

SPATIAL_UNITY_API SpatialUnityWorldHandle SpatialUnity_CreateWorld();
SPATIAL_UNITY_API void SpatialUnity_DestroyWorld(SpatialUnityWorldHandle world);

SPATIAL_UNITY_API SpatialUnityResult SpatialUnity_LoadDataset(SpatialUnityWorldHandle world, const char* manifestPath, SpatialUnityLoadConfig config);
SPATIAL_UNITY_API void SpatialUnity_Shutdown(SpatialUnityWorldHandle world);
SPATIAL_UNITY_API int32_t SpatialUnity_IsLoaded(SpatialUnityWorldHandle world);
SPATIAL_UNITY_API std::uint32_t SpatialUnity_GetDatasetMaxLOD(SpatialUnityWorldHandle world);

SPATIAL_UNITY_API void SpatialUnity_SetDebugVisualization(SpatialUnityWorldHandle world, int32_t enabled);
SPATIAL_UNITY_API int32_t SpatialUnity_GetDebugVisualization(SpatialUnityWorldHandle world);

// Advances streaming and processes pending GPU uploads. Call once per frame.
SPATIAL_UNITY_API void SpatialUnity_Update(SpatialUnityWorldHandle world, SpatialUnityCameraParams camera);

// Selects LOD and records this frame's draw commands + debug lines,
// replacing whatever the previous SpatialUnity_Render call recorded. Pull
// them with the functions below immediately afterward, before the next
// SpatialUnity_Update/Render call.
SPATIAL_UNITY_API void SpatialUnity_Render(SpatialUnityWorldHandle world, SpatialUnityCameraParams camera);

SPATIAL_UNITY_API void SpatialUnity_GetStatistics(SpatialUnityWorldHandle world, SpatialUnityStatistics* outStats);

// --- Per-frame draw commands (populated by the last Render() call) ---
SPATIAL_UNITY_API int32_t SpatialUnity_GetDrawCommandCount(SpatialUnityWorldHandle world);
// outMeshIds/outMaterialIds: count int64 elements each. outTransforms16:
// count*16 floats, one row-major 4x4 per command.
SPATIAL_UNITY_API void SpatialUnity_GetDrawCommands(SpatialUnityWorldHandle world, int64_t* outMeshIds, int64_t* outMaterialIds, float* outTransforms16);

// --- Mesh/material geometry, pulled once per id and cached C#-side ---
SPATIAL_UNITY_API int32_t SpatialUnity_GetMeshVertexCount(SpatialUnityWorldHandle world, int64_t meshId);
SPATIAL_UNITY_API int32_t SpatialUnity_GetMeshIndexCount(SpatialUnityWorldHandle world, int64_t meshId);
// outPositions3/outNormals3: vertexCount*3 floats. outUVs2: vertexCount*2
// floats. outIndices: indexCount int32s. Returns 0 if meshId is unknown.
SPATIAL_UNITY_API int32_t SpatialUnity_GetMeshData(SpatialUnityWorldHandle world, int64_t meshId, float* outPositions3, float* outNormals3, float* outUVs2, int32_t* outIndices);

// outRGBA4: 4 floats. Returns 0 if materialId is unknown (0 = "no material").
SPATIAL_UNITY_API int32_t SpatialUnity_GetMaterialColor(SpatialUnityWorldHandle world, int64_t materialId, float* outRGBA4);

// --- Debug tile-bounds lines (populated by the last Render() call) ---
SPATIAL_UNITY_API int32_t SpatialUnity_GetDebugLineVertexCount(SpatialUnityWorldHandle world);
// outPositions3: count*3 floats. outColors4: count*4 floats. Consecutive
// pairs are one line segment, matching IRenderer::drawDebugLines.
SPATIAL_UNITY_API void SpatialUnity_GetDebugLineData(SpatialUnityWorldHandle world, float* outPositions3, float* outColors4);
