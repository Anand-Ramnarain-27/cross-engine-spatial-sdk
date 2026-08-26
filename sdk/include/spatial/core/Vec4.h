#pragma once

#include <cmath>

#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // 4D vector. Mainly used as the homogeneous form of Vec3 (w=1 for points,
    // w=0 for directions) when multiplying through a Mat4, and as the raw
    // (A, B, C, D) coefficients of a plane equation before normalization.
    struct Vec4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        constexpr Vec4() = default;
        constexpr Vec4(float x_, float y_, float z_, float w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
        constexpr Vec4(const Vec3& v, float w_) noexcept : x(v.x), y(v.y), z(v.z), w(w_) {}

        [[nodiscard]] constexpr bool operator==(const Vec4&) const = default;

        constexpr Vec4& operator+=(const Vec4& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
        constexpr Vec4& operator-=(const Vec4& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
        constexpr Vec4& operator*=(float s) noexcept { x *= s; y *= s; z *= s; w *= s; return *this; }

        [[nodiscard]] constexpr Vec3 xyz() const noexcept { return Vec3{x, y, z}; }

        [[nodiscard]] constexpr float lengthSquared() const noexcept { return x * x + y * y + z * z + w * w; }
        [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }
    };

    [[nodiscard]] constexpr Vec4 operator+(Vec4 lhs, const Vec4& rhs) noexcept { lhs += rhs; return lhs; }
    [[nodiscard]] constexpr Vec4 operator-(Vec4 lhs, const Vec4& rhs) noexcept { lhs -= rhs; return lhs; }
    [[nodiscard]] constexpr Vec4 operator*(Vec4 lhs, float s) noexcept { lhs *= s; return lhs; }

    [[nodiscard]] constexpr float dot(const Vec4& a, const Vec4& b) noexcept
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    [[nodiscard]] inline bool nearlyEqual(const Vec4& a, const Vec4& b, float epsilon = 1e-5f) noexcept
    {
        return std::abs(a.x - b.x) <= epsilon && std::abs(a.y - b.y) <= epsilon &&
               std::abs(a.z - b.z) <= epsilon && std::abs(a.w - b.w) <= epsilon;
    }
}
