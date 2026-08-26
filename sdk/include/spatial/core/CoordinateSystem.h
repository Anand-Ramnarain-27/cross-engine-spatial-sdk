#pragma once

#include <optional>
#include <string_view>

namespace spatial::core
{
    enum class CoordinateSystem
    {
        LocalCartesian,
    };

    [[nodiscard]] constexpr std::string_view toString(CoordinateSystem cs) noexcept
    {
        switch (cs)
        {
            case CoordinateSystem::LocalCartesian:
                return "LOCAL_CARTESIAN";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] inline std::optional<CoordinateSystem> coordinateSystemFromString(std::string_view s) noexcept
    {
        if (s == "LOCAL_CARTESIAN")
        {
            return CoordinateSystem::LocalCartesian;
        }
        return std::nullopt;
    }
}
