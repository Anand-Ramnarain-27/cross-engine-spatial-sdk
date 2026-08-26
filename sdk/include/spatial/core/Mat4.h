#pragma once

#include <cmath>

#include "spatial/core/Vec3.h"
#include "spatial/core/Vec4.h"

namespace spatial::core
{
    // 4x4 matrix.
    //
    // Convention (fixed for the whole SDK — engine integration layers are
    // responsible for converting to/from whatever convention their engine
    // uses, since that conversion is exactly the adapter's job):
    //   - Storage is logically row-major: m[row][col].
    //   - Vectors are column vectors: a transform is applied as v' = M * v.
    //   - Composition (A * B) means "apply B first, then A", matching
    //     standard matrix algebra: v' = (A * B) * v = A * (B * v).
    //   - Right-handed coordinate system; perspective() targets an NDC depth
    //     range of [-1, 1] (OpenGL-style), chosen because it makes the
    //     frustum plane extraction formulas unambiguous and easy to verify.
    struct Mat4
    {
        float m[4][4] = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };

        [[nodiscard]] static constexpr Mat4 identity() noexcept { return Mat4{}; }

        [[nodiscard]] static constexpr Mat4 translation(const Vec3& t) noexcept
        {
            Mat4 result{};
            result.m[0][3] = t.x;
            result.m[1][3] = t.y;
            result.m[2][3] = t.z;
            return result;
        }

        [[nodiscard]] static constexpr Mat4 scale(const Vec3& s) noexcept
        {
            Mat4 result{};
            result.m[0][0] = s.x;
            result.m[1][1] = s.y;
            result.m[2][2] = s.z;
            return result;
        }

        // fovYRadians: full vertical field of view. aspect: width / height.
        [[nodiscard]] static Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ) noexcept
        {
            const float f = 1.0f / std::tan(fovYRadians * 0.5f);
            Mat4 result{};
            result.m[0][0] = f / aspect;
            result.m[1][1] = f;
            result.m[2][2] = (farZ + nearZ) / (nearZ - farZ);
            result.m[2][3] = (2.0f * farZ * nearZ) / (nearZ - farZ);
            result.m[3][2] = -1.0f;
            result.m[3][3] = 0.0f;
            return result;
        }

        // Right-handed view matrix: world-space eye looking toward target.
        [[nodiscard]] static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept
        {
            const Vec3 f = (target - eye).normalized();
            const Vec3 s = cross(f, up).normalized();
            const Vec3 u = cross(s, f);

            Mat4 result{};
            result.m[0][0] = s.x; result.m[0][1] = s.y; result.m[0][2] = s.z; result.m[0][3] = -dot(s, eye);
            result.m[1][0] = u.x; result.m[1][1] = u.y; result.m[1][2] = u.z; result.m[1][3] = -dot(u, eye);
            result.m[2][0] = -f.x; result.m[2][1] = -f.y; result.m[2][2] = -f.z; result.m[2][3] = dot(f, eye);
            result.m[3][0] = 0.0f; result.m[3][1] = 0.0f; result.m[3][2] = 0.0f; result.m[3][3] = 1.0f;
            return result;
        }

        [[nodiscard]] constexpr Mat4 operator*(const Mat4& rhs) const noexcept
        {
            Mat4 result{};
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        sum += m[row][k] * rhs.m[k][col];
                    }
                    result.m[row][col] = sum;
                }
            }
            return result;
        }

        [[nodiscard]] constexpr Vec4 transform(const Vec4& v) const noexcept
        {
            return Vec4{
                m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
                m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w,
            };
        }

        // Transforms a point (implicit w=1) and divides by the resulting w,
        // so this is safe to use directly with a perspective matrix.
        [[nodiscard]] constexpr Vec3 transformPoint(const Vec3& p) const noexcept
        {
            const Vec4 r = transform(Vec4{p, 1.0f});
            if (r.w == 0.0f)
            {
                return r.xyz();
            }
            return r.xyz() / r.w;
        }

        // Transforms a direction (implicit w=0): ignores translation.
        [[nodiscard]] constexpr Vec3 transformVector(const Vec3& v) const noexcept
        {
            return transform(Vec4{v, 0.0f}).xyz();
        }

        [[nodiscard]] constexpr float operator()(int row, int col) const noexcept { return m[row][col]; }
    };
}
