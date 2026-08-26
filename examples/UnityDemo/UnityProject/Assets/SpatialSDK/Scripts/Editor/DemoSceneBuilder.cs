using SpatialSDK;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace SpatialSDK.EditorTools
{
    // Builds examples/UnityDemo/UnityProject's demo scene from scratch —
    // a camera positioned to see the generated DemoCity dataset, a light,
    // and a GameObject with SpatialWorldComponent. Runnable from the
    // command line (`-executeMethod SpatialSDK.EditorTools.DemoSceneBuilder.Build`)
    // so the scene is reproducible rather than a hand-authored .unity file
    // nobody can regenerate.
    public static class DemoSceneBuilder
    {
        [MenuItem("Spatial SDK/Rebuild Demo Scene")]
        public static void Build()
        {
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

            var lightGO = new GameObject("Directional Light");
            Light light = lightGO.AddComponent<Light>();
            light.type = LightType.Directional;
            light.intensity = 1.2f;
            lightGO.transform.rotation = Quaternion.Euler(50f, -30f, 0f);

            var cameraGO = new GameObject("Main Camera");
            Camera camera = cameraGO.AddComponent<Camera>();
            camera.tag = "MainCamera";
            camera.fieldOfView = 60f;
            camera.nearClipPlane = 0.3f;
            camera.farClipPlane = 2000f;
            // DemoCity is a 4x4 grid of 50m tiles centered on the origin
            // (worldSize = 200) — this framing sees the whole thing.
            cameraGO.transform.position = new Vector3(0f, 90f, -160f);
            cameraGO.transform.rotation = Quaternion.Euler(28f, 0f, 0f);

            var worldGO = new GameObject("SpatialWorld");
            SpatialWorldComponent worldComponent = worldGO.AddComponent<SpatialWorldComponent>();
            worldComponent.material = CreateDefaultMaterial();

            EditorSceneManager.MoveGameObjectToScene(lightGO, scene);
            EditorSceneManager.MoveGameObjectToScene(cameraGO, scene);
            EditorSceneManager.MoveGameObjectToScene(worldGO, scene);

            const string scenePath = "Assets/Scenes/SpatialSDKDemo.unity";
            EditorSceneManager.SaveScene(scene, scenePath);
            EditorBuildSettings.scenes = new[] { new EditorBuildSettingsScene(scenePath, true) };

            Debug.Log($"DemoSceneBuilder: wrote {scenePath}");
        }

        private static Material CreateDefaultMaterial()
        {
            Shader shader = Shader.Find("Universal Render Pipeline/Lit") ?? Shader.Find("Standard") ?? Shader.Find("Diffuse");
            var material = new Material(shader) { name = "SpatialTileDefault" };
            AssetDatabase.CreateAsset(material, "Assets/SpatialSDK/SpatialTileDefault.mat");
            return material;
        }
    }
}
