#pragma once

#include <cmath>

namespace spatial::core
{
    // 3D vector. The primary world-space quantity used throughout Core, Data,
    // Streaming, and Culling.
    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vec3() = default;
        constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

        [[nodiscard]] constexpr bool operator==(const Vec3&) const = default;

        constexpr Vec3& operator+=(const Vec3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
        constexpr Vec3& operator-=(const Vec3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
        constexpr Vec3& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
        constexpr Vec3& operator/=(float s) noexcept { x /= s; y /= s; z /= s; return *this; }

        [[nodiscard]] constexpr float lengthSquared() const noexcept { return x * x + y * y + z * z; }
        [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }

        // Returns a zero vector if this vector's length is zero, rather than
        // producing NaN, so callers don't need to special-case degenerate input.
        [[nodiscard]] Vec3 normalized() const noexcept
        {
            const float len = length();
            if (len <= 0.0f)
            {
                return Vec3{};
            }
            return Vec3{x / len, y / len, z / len};
        }

        [[nodiscard]] static constexpr Vec3 zero() noexcept { return Vec3{0.0f, 0.0f, 0.0f}; }
        [[nodiscard]] static constexpr Vec3 one() noexcept { return Vec3{1.0f, 1.0f, 1.0f}; }
        [[nodiscard]] static constexpr Vec3 unitX() noexcept { return Vec3{1.0f, 0.0f, 0.0f}; }
        [[nodiscard]] static constexpr Vec3 unitY() noexcept { return Vec3{0.0f, 1.0f, 0.0f}; }
        [[nodiscard]] static constexpr Vec3 unitZ() noexcept { return Vec3{0.0f, 0.0f, 1.0f}; }
    };

    [[nodiscard]] constexpr Vec3 operator+(Vec3 lhs, const Vec3& rhs) noexcept { lhs += rhs; return lhs; }
    [[nodiscard]] constexpr Vec3 operator-(Vec3 lhs, const Vec3& rhs) noexcept { lhs -= rhs; return lhs; }
    [[nodiscard]] constexpr Vec3 operator*(Vec3 lhs, float s) noexcept { lhs *= s; return lhs; }
    [[nodiscard]] constexpr Vec3 operator*(float s, Vec3 rhs) noexcept { rhs *= s; return rhs; }
    [[nodiscard]] constexpr Vec3 operator/(Vec3 lhs, float s) noexcept { lhs /= s; return lhs; }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& v) noexcept { return Vec3{-v.x, -v.y, -v.z}; }

    // Componentwise min/max, used by AABB construction.
    [[nodiscard]] constexpr Vec3 minComponents(const Vec3& a, const Vec3& b) noexcept
    {
        return Vec3{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
    }

    [[nodiscard]] constexpr Vec3 maxComponents(const Vec3& a, const Vec3& b) noexcept
    {
        return Vec3{a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
    }

    [[nodiscard]] constexpr float dot(const Vec3& a, const Vec3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

    [[nodiscard]] constexpr Vec3 cross(const Vec3& a, const Vec3& b) noexcept
    {
        return Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    }

    [[nodiscard]] inline float distance(const Vec3& a, const Vec3& b) noexcept { return (a - b).length(); }
    [[nodiscard]] inline float distanceSquared(const Vec3& a, const Vec3& b) noexcept { return (a - b).lengthSquared(); }

    [[nodiscard]] inline bool nearlyEqual(const Vec3& a, const Vec3& b, float epsilon = 1e-5f) noexcept
    {
        return std::abs(a.x - b.x) <= epsilon && std::abs(a.y - b.y) <= epsilon && std::abs(a.z - b.z) <= epsilon;
    }
}
