#pragma once

#include <cmath>

namespace spatial::core
{
    // 2D vector. Used for screen-space / UV quantities (e.g. viewport size,
    // screen-space error) rather than world-space geometry.
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vec2() = default;
        constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

        [[nodiscard]] constexpr bool operator==(const Vec2&) const = default;

        constexpr Vec2& operator+=(const Vec2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        constexpr Vec2& operator-=(const Vec2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
        constexpr Vec2& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
        constexpr Vec2& operator/=(float s) noexcept { x /= s; y /= s; return *this; }

        [[nodiscard]] constexpr float lengthSquared() const noexcept { return x * x + y * y; }
        [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }

        // Returns a zero vector if this vector's length is zero, rather than
        // producing NaN, so callers don't need to special-case degenerate input.
        [[nodiscard]] Vec2 normalized() const noexcept
        {
            const float len = length();
            if (len <= 0.0f)
            {
                return Vec2{};
            }
            return Vec2{x / len, y / len};
        }
    };

    [[nodiscard]] constexpr Vec2 operator+(Vec2 lhs, const Vec2& rhs) noexcept { lhs += rhs; return lhs; }
    [[nodiscard]] constexpr Vec2 operator-(Vec2 lhs, const Vec2& rhs) noexcept { lhs -= rhs; return lhs; }
    [[nodiscard]] constexpr Vec2 operator*(Vec2 lhs, float s) noexcept { lhs *= s; return lhs; }
    [[nodiscard]] constexpr Vec2 operator*(float s, Vec2 rhs) noexcept { rhs *= s; return rhs; }
    [[nodiscard]] constexpr Vec2 operator/(Vec2 lhs, float s) noexcept { lhs /= s; return lhs; }
    [[nodiscard]] constexpr Vec2 operator-(const Vec2& v) noexcept { return Vec2{-v.x, -v.y}; }

    [[nodiscard]] constexpr float dot(const Vec2& a, const Vec2& b) noexcept { return a.x * b.x + a.y * b.y; }

    [[nodiscard]] inline bool nearlyEqual(const Vec2& a, const Vec2& b, float epsilon = 1e-5f) noexcept
    {
        return std::abs(a.x - b.x) <= epsilon && std::abs(a.y - b.y) <= epsilon;
    }
}
