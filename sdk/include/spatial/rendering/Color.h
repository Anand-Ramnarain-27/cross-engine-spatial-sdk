#pragma once

namespace spatial::rendering
{
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        [[nodiscard]] constexpr bool operator==(const Color&) const = default;
    };
}
