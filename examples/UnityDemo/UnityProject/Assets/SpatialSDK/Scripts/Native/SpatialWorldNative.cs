using System;
using System.Runtime.InteropServices;

namespace SpatialSDK.Native
{
    // Mirrors examples/UnityDemo/NativePlugin/src/SpatialUnityPlugin.h field
    // for field. This is the only file that should contain [DllImport] —
    // everything else in SpatialSDK talks to SpatialWorldNative, never to
    // the native library directly. Keep this in sync by hand: there is no
    // codegen step, the same way the C++ header comment says.
    public enum SpatialUnityResult
    {
        Ok = 0,
        DatasetNotFound = 1,
        InvalidDataset = 2,
        UnsupportedVersion = 3,
        TileLoadFailed = 4,
        CorruptTile = 5,
        OutOfMemory = 6,
        GPUUploadFailed = 7,
        InvalidState = 8,
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpatialUnityCameraParams
    {
        public float posX, posY, posZ;
        public float fwdX, fwdY, fwdZ;
        public float verticalFovRadians;
        public float viewportHeightPx;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpatialUnityLoadConfig
    {
        public float streamingRadius;
        public uint maxResidentTiles;
        public ulong cpuMemoryBudgetBytes;
        public uint workerThreadCount;
        public uint maxGPUUploadsPerUpdate;
        public int debugVisualizationEnabled;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpatialUnityStatistics
    {
        public ulong requestedCount;
        public ulong loadingCount;
        public ulong residentCount;
        public ulong cpuMemoryUsedBytes;
        public ulong gpuMemoryUsedBytes;
        public ulong totalLoadsCompleted;
        public ulong totalLoadsFailed;
        public ulong totalCancellations;
        public ulong totalUnloads;
        public ulong totalCacheHits;
    }

    public static class SpatialWorldNative
    {
        private const string PluginName = "SpatialUnityPlugin";

        [DllImport(PluginName)]
        public static extern IntPtr SpatialUnity_CreateWorld();

        [DllImport(PluginName)]
        public static extern void SpatialUnity_DestroyWorld(IntPtr world);

        [DllImport(PluginName, CharSet = CharSet.Ansi)]
        public static extern SpatialUnityResult SpatialUnity_LoadDataset(IntPtr world, [MarshalAs(UnmanagedType.LPStr)] string manifestPath, SpatialUnityLoadConfig config);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_Shutdown(IntPtr world);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_IsLoaded(IntPtr world);

        [DllImport(PluginName)]
        public static extern uint SpatialUnity_GetDatasetMaxLOD(IntPtr world);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_SetDebugVisualization(IntPtr world, int enabled);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetDebugVisualization(IntPtr world);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_Update(IntPtr world, SpatialUnityCameraParams camera);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_Render(IntPtr world, SpatialUnityCameraParams camera);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_GetStatistics(IntPtr world, out SpatialUnityStatistics outStats);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetDrawCommandCount(IntPtr world);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_GetDrawCommands(IntPtr world, [In, Out] long[] outMeshIds, [In, Out] long[] outMaterialIds, [In, Out] float[] outTransforms16);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetMeshVertexCount(IntPtr world, long meshId);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetMeshIndexCount(IntPtr world, long meshId);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetMeshData(IntPtr world, long meshId, [In, Out] float[] outPositions3, [In, Out] float[] outNormals3, [In, Out] float[] outUVs2, [In, Out] int[] outIndices);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetMaterialColor(IntPtr world, long materialId, [In, Out] float[] outRGBA4);

        [DllImport(PluginName)]
        public static extern int SpatialUnity_GetDebugLineVertexCount(IntPtr world);

        [DllImport(PluginName)]
        public static extern void SpatialUnity_GetDebugLineData(IntPtr world, [In, Out] float[] outPositions3, [In, Out] float[] outColors4);
    }
}
