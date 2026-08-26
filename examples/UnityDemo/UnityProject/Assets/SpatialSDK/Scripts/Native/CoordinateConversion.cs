using UnityEngine;

namespace SpatialSDK.Native
{
    // The core SDK's spatial::core::Mat4 is documented (sdk/include/spatial/core/Mat4.h)
    // as right-handed, Y-up. Unity is left-handed, Y-up. Same up axis, opposite
    // handedness — the standard fix (also what glTF-for-Unity importers do) is
    // to negate Z on every position/direction/normal crossing the boundary and
    // reverse triangle winding to compensate, rather than touching angles or
    // magnitudes. Every native<->Unity coordinate conversion goes through here
    // so there is exactly one place this convention lives.
    public static class CoordinateConversion
    {
        public static Vector3 ToUnityPosition(float x, float y, float z) => new Vector3(x, y, -z);

        public static Vector3 ToUnityDirection(float x, float y, float z) => new Vector3(x, y, -z);

        public static SpatialUnityCameraParams ToNativeCamera(Vector3 unityPosition, Vector3 unityForward, float verticalFovRadians, float viewportHeightPx)
        {
            // Negation is self-inverse, so the Unity->native direction is the
            // same formula as ToUnityPosition/ToUnityDirection above.
            return new SpatialUnityCameraParams
            {
                posX = unityPosition.x,
                posY = unityPosition.y,
                posZ = -unityPosition.z,
                fwdX = unityForward.x,
                fwdY = unityForward.y,
                fwdZ = -unityForward.z,
                verticalFovRadians = verticalFovRadians,
                viewportHeightPx = viewportHeightPx,
            };
        }

        // M' = F * M * F, F = diag(1,1,-1,1) — conjugating a right-handed
        // transform by the same Z-mirror applied to the geometry it moves.
        // Always the identity today (SpatialWorld::render() never passes a
        // non-identity worldTransform — see ManagedMeshRenderer.h) but kept
        // general rather than special-cased, since it's cheap either way.
        public static Matrix4x4 ToUnityMatrix(float[] rowMajor16, int offset)
        {
            Matrix4x4 m = default;
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    float value = rowMajor16[offset + (row * 4) + col];
                    bool flipRow = row == 2;
                    bool flipCol = col == 2;
                    if (flipRow ^ flipCol)
                    {
                        value = -value;
                    }
                    m[row, col] = value;
                }
            }
            return m;
        }
    }
}
