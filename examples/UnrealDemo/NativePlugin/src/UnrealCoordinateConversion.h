#pragma once

// Right-handed, Y-up, meters (spatial::core::Mat4's convention) <-> Unreal's
// left-handed, Z-up, centimeters. Unlike examples/UnityDemo (where the
// equivalent conversion lives in C# — see CoordinateConversion.cs), this one
// lives entirely in the native plugin: every SpatialUnreal_Get* function
// hands back Unreal-space-ready data, so the actual Unreal C++ component
// does zero conversion math of its own. That's a deliberate refinement over
// the Unity plugin, made possible by the conversion needing this exact
// binary (not a host language) either way — and it means this math, the
// single most error-prone part of any engine boundary, is provable by this
// repo's own Catch2 suite instead of only by looking at the screen (see
// tests/examples/SpatialUnrealPluginTests.cpp).
//
// The conversion is a pure axis permutation — swap the Y and Z components,
// leave X alone — plus a 100x scale on positions (meters -> centimeters;
// directions/normals/rotations are unaffected by the scale). Swapping
// exactly two axes is an odd permutation, which by itself both converts
// Y-up to Z-up *and* flips handedness (right-handed -> left-handed) with no
// separate sign negation needed — this is the same convention glTF (RH,
// Y-up) importers for Unreal use. Flipping handedness also flips front-face
// triangle winding, which is why every mesh-pulling function reverses each
// triangle's winding to compensate (see ManagedMeshRenderer's comment on
// why the SDK's procedurally generated geometry is CCW-front-facing).

#include "spatial/core/Mat4.h"
#include "spatial/core/Vec3.h"

namespace spatial::examples::unreal_convert
{
    constexpr float kMetersToCentimeters = 100.0f;

    // spatial-space -> Unreal-space.
    inline void position(float x, float y, float z, float& outX, float& outY, float& outZ) noexcept
    {
        outX = x * kMetersToCentimeters;
        outY = z * kMetersToCentimeters;
        outZ = y * kMetersToCentimeters;
    }

    inline void direction(float x, float y, float z, float& outX, float& outY, float& outZ) noexcept
    {
        outX = x;
        outY = z;
        outZ = y;
    }

    // Unreal-space -> spatial-space (the inverse of position()/direction() —
    // negation-free permutations are self-inverse, so it's the same swap).
    [[nodiscard]] inline core::Vec3 toSpatialPosition(float x, float y, float z) noexcept
    {
        return core::Vec3{x / kMetersToCentimeters, z / kMetersToCentimeters, y / kMetersToCentimeters};
    }

    [[nodiscard]] inline core::Vec3 toSpatialDirection(float x, float y, float z) noexcept
    {
        return core::Vec3{x, z, y};
    }

    // M_ue[i][j] = M_sdk[perm(i)][perm(j)], translation columns (j == 3)
    // additionally scaled by 100 — see this file's header comment for the
    // derivation. Always the identity in practice today (SpatialWorld never
    // passes a non-identity worldTransform), kept general regardless.
    [[nodiscard]] inline core::Mat4 transform(const core::Mat4& sdk) noexcept
    {
        constexpr int perm[4] = {0, 2, 1, 3};
        core::Mat4 result{};
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float value = sdk.m[perm[i]][perm[j]];
                if (j == 3 && i != 3)
                {
                    value *= kMetersToCentimeters;
                }
                result.m[i][j] = value;
            }
        }
        return result;
    }
}
