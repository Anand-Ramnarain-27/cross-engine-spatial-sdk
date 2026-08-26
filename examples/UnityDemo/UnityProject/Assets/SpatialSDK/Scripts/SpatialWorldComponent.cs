using System;
using System.Collections.Generic;
using System.IO;
using SpatialSDK.Native;
using UnityEngine;

namespace SpatialSDK
{
    // The Unity-side half of the Phase 10 integration: native plugin (C++,
    // wraps spatial::SpatialWorld) <- SpatialWorldNative (P/Invoke) <- this
    // component. No core SDK logic lives here — streaming, LOD selection,
    // and the tile/resource state machine all run in C++; this class only
    // marshals camera state in, pulls draw commands out, and turns them into
    // UnityEngine.Mesh objects drawn with Graphics.DrawMesh (see
    // examples/UnityDemo/README.md for why that approach and not a native
    // graphics-API plugin).
    [DisallowMultipleComponent]
    [AddComponentMenu("Spatial SDK/Spatial World")]
    public class SpatialWorldComponent : MonoBehaviour
    {
        [Header("Dataset")]
        [Tooltip("Path to the .world manifest, relative to Assets/StreamingAssets/SpatialSDK.")]
        public string datasetPath = "DemoCity/DemoCity.world";

        [Header("Streaming")]
        public float streamingRadius = 400.0f;
        public uint maxResidentTiles = 256;
        public uint cpuMemoryBudgetMB = 512;
        public uint workerThreadCount = 4;
        public uint maxGPUUploadsPerUpdate = 16;

        [Header("Visualization")]
        public bool enableDebugVisualization = true;
        [Tooltip("Draws a resident/loading/requested tile count + memory overlay via OnGUI.")]
        public bool enableStatistics = true;
        [Tooltip("Shared material tiles are drawn with; per-draw base color comes from the tile's material via a MaterialPropertyBlock.")]
        public Material material;

        [Header("Camera")]
        [Tooltip("Drives streaming and LOD selection. Defaults to Camera.main if left unset.")]
        public Transform cameraOverride;

        // "Maximum LOD" (the brief's suggested inspector property) is
        // dataset-derived, not a runtime knob — the SDK doesn't have a
        // concept of capping LOD selection independent of the dataset's own
        // level count. Surfaced read-only here and in the stats overlay
        // rather than faked as an editable field.
        public uint DatasetMaxLOD { get; private set; }
        public bool IsLoaded { get; private set; }

        private IntPtr m_world = IntPtr.Zero;
        private Material m_lineMaterial;

        private readonly Dictionary<long, Mesh> m_meshCache = new();
        private readonly Dictionary<long, MaterialPropertyBlock> m_materialPropertyCache = new();

        private long[] m_meshIds = Array.Empty<long>();
        private long[] m_materialIds = Array.Empty<long>();
        private float[] m_transforms = Array.Empty<float>();

        private float[] m_debugPositions = Array.Empty<float>();
        private float[] m_debugColors = Array.Empty<float>();
        private int m_debugLineVertexCount;

        private SpatialUnityStatistics m_lastStatistics;

        private void Awake()
        {
            m_world = SpatialWorldNative.SpatialUnity_CreateWorld();
        }

        private void Start()
        {
            string fullPath = Path.Combine(Application.streamingAssetsPath, "SpatialSDK", datasetPath);

            var config = new SpatialUnityLoadConfig
            {
                streamingRadius = streamingRadius,
                maxResidentTiles = maxResidentTiles,
                cpuMemoryBudgetBytes = (ulong)cpuMemoryBudgetMB * 1024ul * 1024ul,
                workerThreadCount = workerThreadCount,
                maxGPUUploadsPerUpdate = maxGPUUploadsPerUpdate,
                debugVisualizationEnabled = enableDebugVisualization ? 1 : 0,
            };

            SpatialUnityResult result = SpatialWorldNative.SpatialUnity_LoadDataset(m_world, fullPath, config);
            IsLoaded = result == SpatialUnityResult.Ok;
            if (!IsLoaded)
            {
                Debug.LogError($"SpatialWorldComponent: failed to load dataset '{fullPath}' ({result}).", this);
                return;
            }

            DatasetMaxLOD = SpatialWorldNative.SpatialUnity_GetDatasetMaxLOD(m_world);
        }

        private void Update()
        {
            if (!IsLoaded)
            {
                return;
            }

            Transform cameraTransform = ResolveCameraTransform();
            if (cameraTransform == null)
            {
                return;
            }

            float verticalFov = Camera.main != null ? Camera.main.fieldOfView * Mathf.Deg2Rad : 60f * Mathf.Deg2Rad;
            SpatialUnityCameraParams camera = CoordinateConversion.ToNativeCamera(cameraTransform.position, cameraTransform.forward, verticalFov, Screen.height);

            SpatialWorldNative.SpatialUnity_SetDebugVisualization(m_world, enableDebugVisualization ? 1 : 0);
            SpatialWorldNative.SpatialUnity_Update(m_world, camera);
            SpatialWorldNative.SpatialUnity_Render(m_world, camera);

            DrawResidentTiles();
            PullDebugLines();

            if (enableStatistics)
            {
                SpatialWorldNative.SpatialUnity_GetStatistics(m_world, out m_lastStatistics);
            }
        }

        private Transform ResolveCameraTransform()
        {
            if (cameraOverride != null)
            {
                return cameraOverride;
            }
            return Camera.main != null ? Camera.main.transform : null;
        }

        private void DrawResidentTiles()
        {
            int drawCount = SpatialWorldNative.SpatialUnity_GetDrawCommandCount(m_world);
            if (drawCount == 0)
            {
                return;
            }

            if (m_meshIds.Length < drawCount)
            {
                m_meshIds = new long[drawCount];
                m_materialIds = new long[drawCount];
                m_transforms = new float[drawCount * 16];
            }

            SpatialWorldNative.SpatialUnity_GetDrawCommands(m_world, m_meshIds, m_materialIds, m_transforms);

            for (int i = 0; i < drawCount; ++i)
            {
                Mesh mesh = GetOrBuildMesh(m_meshIds[i]);
                if (mesh == null)
                {
                    continue;
                }

                Matrix4x4 matrix = CoordinateConversion.ToUnityMatrix(m_transforms, i * 16);
                MaterialPropertyBlock properties = GetOrBuildMaterialProperties(m_materialIds[i]);
                Material drawMaterial = material != null ? material : GetLineMaterial();

                Graphics.DrawMesh(mesh, matrix, drawMaterial, gameObject.layer, null, 0, properties);
            }
        }

        private Mesh GetOrBuildMesh(long meshId)
        {
            if (m_meshCache.TryGetValue(meshId, out Mesh cached))
            {
                return cached;
            }

            int vertexCount = SpatialWorldNative.SpatialUnity_GetMeshVertexCount(m_world, meshId);
            int indexCount = SpatialWorldNative.SpatialUnity_GetMeshIndexCount(m_world, meshId);
            if (vertexCount == 0 || indexCount == 0)
            {
                return null;
            }

            var positions = new float[vertexCount * 3];
            var normals = new float[vertexCount * 3];
            var uvs = new float[vertexCount * 2];
            var indices = new int[indexCount];
            if (SpatialWorldNative.SpatialUnity_GetMeshData(m_world, meshId, positions, normals, uvs, indices) == 0)
            {
                return null;
            }

            var vertexArray = new Vector3[vertexCount];
            var normalArray = new Vector3[vertexCount];
            var uvArray = new Vector2[vertexCount];
            for (int i = 0; i < vertexCount; ++i)
            {
                vertexArray[i] = CoordinateConversion.ToUnityPosition(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
                normalArray[i] = CoordinateConversion.ToUnityDirection(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
                uvArray[i] = new Vector2(uvs[i * 2 + 0], uvs[i * 2 + 1]);
            }

            // The Z-negation above mirrors the mesh, which also flips winding
            // from front-face to back-face; swapping each triangle's last two
            // indices restores correct front-face culling in Unity.
            var triangleArray = new int[indexCount];
            for (int i = 0; i + 2 < indexCount; i += 3)
            {
                triangleArray[i + 0] = indices[i + 0];
                triangleArray[i + 1] = indices[i + 2];
                triangleArray[i + 2] = indices[i + 1];
            }

            var mesh = new Mesh { name = $"SpatialTileMesh_{meshId}" };
            mesh.SetVertices(vertexArray);
            mesh.SetNormals(normalArray);
            mesh.SetUVs(0, uvArray);
            mesh.SetTriangles(triangleArray, 0);
            mesh.RecalculateBounds();

            m_meshCache[meshId] = mesh;
            return mesh;
        }

        private MaterialPropertyBlock GetOrBuildMaterialProperties(long materialId)
        {
            if (m_materialPropertyCache.TryGetValue(materialId, out MaterialPropertyBlock cached))
            {
                return cached;
            }

            var properties = new MaterialPropertyBlock();
            var rgba = new float[4] { 1f, 1f, 1f, 1f };
            if (materialId != 0 && SpatialWorldNative.SpatialUnity_GetMaterialColor(m_world, materialId, rgba) != 0)
            {
                // Built-in RP's Standard shader reads "_Color"; URP/HDRP Lit
                // reads "_BaseColor". Setting both means the same procedural
                // color works no matter which pipeline's default material the
                // Inspector's `material` field is assigned.
                var color = new Color(rgba[0], rgba[1], rgba[2], rgba[3]);
                properties.SetColor(BaseColorPropertyId, color);
                properties.SetColor(LegacyColorPropertyId, color);
            }

            m_materialPropertyCache[materialId] = properties;
            return properties;
        }

        private static readonly int BaseColorPropertyId = Shader.PropertyToID("_BaseColor");
        private static readonly int LegacyColorPropertyId = Shader.PropertyToID("_Color");

        private void PullDebugLines()
        {
            m_debugLineVertexCount = 0;
            if (!enableDebugVisualization)
            {
                return;
            }

            int vertexCount = SpatialWorldNative.SpatialUnity_GetDebugLineVertexCount(m_world);
            if (vertexCount == 0)
            {
                return;
            }

            if (m_debugPositions.Length < vertexCount * 3)
            {
                m_debugPositions = new float[vertexCount * 3];
                m_debugColors = new float[vertexCount * 4];
            }

            SpatialWorldNative.SpatialUnity_GetDebugLineData(m_world, m_debugPositions, m_debugColors);
            m_debugLineVertexCount = vertexCount;
        }

        private Material GetLineMaterial()
        {
            if (m_lineMaterial == null)
            {
                // Unity's built-in vertex-colored unlit shader — the standard
                // choice for GL immediate-mode drawing (also what Gizmos use).
                m_lineMaterial = new Material(Shader.Find("Hidden/Internal-Colored"))
                {
                    hideFlags = HideFlags.HideAndDontSave,
                };
                m_lineMaterial.SetInt("_ZWrite", 1);
            }
            return m_lineMaterial;
        }

        private void OnRenderObject()
        {
            if (m_debugLineVertexCount == 0)
            {
                return;
            }

            Material lineMaterial = GetLineMaterial();
            lineMaterial.SetPass(0);

            GL.PushMatrix();
            GL.Begin(GL.LINES);
            for (int i = 0; i < m_debugLineVertexCount; ++i)
            {
                GL.Color(new Color(m_debugColors[i * 4 + 0], m_debugColors[i * 4 + 1], m_debugColors[i * 4 + 2], m_debugColors[i * 4 + 3]));
                GL.Vertex(CoordinateConversion.ToUnityPosition(m_debugPositions[i * 3 + 0], m_debugPositions[i * 3 + 1], m_debugPositions[i * 3 + 2]));
            }
            GL.End();
            GL.PopMatrix();
        }

        private void OnGUI()
        {
            if (!enableStatistics || !IsLoaded)
            {
                return;
            }

            const int width = 300;
            GUI.Box(new Rect(10, 10, width, 160), GUIContent.none);
            GUILayout.BeginArea(new Rect(20, 18, width - 20, 150));
            GUILayout.Label($"Dataset max LOD: {DatasetMaxLOD}");
            GUILayout.Label($"Resident tiles: {m_lastStatistics.residentCount}");
            GUILayout.Label($"Loading tiles: {m_lastStatistics.loadingCount}");
            GUILayout.Label($"Requested tiles: {m_lastStatistics.requestedCount}");
            GUILayout.Label($"CPU memory: {m_lastStatistics.cpuMemoryUsedBytes / (1024f * 1024f):F1} MB");
            GUILayout.Label($"GPU memory: {m_lastStatistics.gpuMemoryUsedBytes / (1024f * 1024f):F1} MB");
            GUILayout.Label($"Loads completed: {m_lastStatistics.totalLoadsCompleted}");
            GUILayout.Label($"Cache hits: {m_lastStatistics.totalCacheHits}");
            GUILayout.EndArea();
        }

        private void OnDestroy()
        {
            if (m_world == IntPtr.Zero)
            {
                return;
            }

            // Shutdown() releases every GPU-side resource ManagedMeshRenderer
            // tracks before DestroyWorld() frees the PluginState (renderer +
            // world) that owns them — mirrors the C++ side's own lifetime
            // rule (renderer declared/destroyed after world it backs).
            SpatialWorldNative.SpatialUnity_Shutdown(m_world);
            SpatialWorldNative.SpatialUnity_DestroyWorld(m_world);
            m_world = IntPtr.Zero;

            if (m_lineMaterial != null)
            {
                DestroyImmediate(m_lineMaterial);
            }
        }
    }
}
