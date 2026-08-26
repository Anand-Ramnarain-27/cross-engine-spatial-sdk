# UnityDemo

Unity native-plugin integration (Phase 10). Full design and rationale in
[docs/unity_integration.md](../../docs/unity_integration.md).

```
NativePlugin/          C++ CMake target -> SpatialUnityPlugin.dll (the C ABI + ManagedMeshRenderer)
UnityProject/           a real, openable Unity project
```

## Try it

1. Build the plugin (part of the normal top-level build; requires Windows):

   ```bash
   cmake --build build --config Debug --target SpatialUnityPlugin
   ```

   This copies `SpatialUnityPlugin.dll` into
   `UnityProject/Assets/Plugins/x86_64/` automatically.

2. Generate the demo dataset (not committed — regenerate it):

   ```bash
   build/bin/Debug/SpatialTileBuilder --output examples/UnityDemo/UnityProject/Assets/StreamingAssets/SpatialSDK/DemoCity --name DemoCity --grid 4 --tile-size 50 --max-lod 2 --buildings-per-tile 3 --seed 42
   ```

3. Open `UnityProject/` in Unity Hub (tested against Unity 6000.5.9f1) and
   open `Assets/Scenes/SpatialSDKDemo.unity`. Enter Play mode.

Verified end-to-end in the real Unity Editor: the scene streams the
generated dataset, resident tiles turn into real `UnityEngine.Mesh`
geometry drawn with `Graphics.DrawMesh`, buildings render solid and
correctly shaded (confirming the right-handed-to-Unity coordinate
conversion — including triangle winding — is correct, not mirrored or
inside-out), the green tile-bounds debug overlay matches
`StandaloneViewer`'s, and the `OnGUI` stats overlay updates live
(`Resident tiles: 16`, `Loading tiles: 0` at steady state for this
dataset). Zero console errors or warnings. If `Assets/Scenes/` is ever
lost or you want to rebuild it from scratch, `Spatial SDK > Rebuild Demo
Scene` in the Editor menu regenerates it
(`Assets/SpatialSDK/Scripts/Editor/DemoSceneBuilder.cs`).

## Layout

```
NativePlugin/src/
  SpatialUnityPlugin.h/.cpp   flat C ABI wrapping spatial::SpatialWorld
  ManagedMeshRenderer.h/.cpp  the IRenderer implementation behind it

UnityProject/Assets/SpatialSDK/Scripts/
  Native/SpatialWorldNative.cs      [DllImport] bindings — mirrors SpatialUnityPlugin.h field-for-field
  Native/CoordinateConversion.cs    the one place right-handed <-> Unity handedness is handled
  SpatialWorldComponent.cs          the MonoBehaviour every scene adds
  Editor/DemoSceneBuilder.cs        regenerates Assets/Scenes/SpatialSDKDemo.unity
```
