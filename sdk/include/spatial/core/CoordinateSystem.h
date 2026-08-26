#pragma once

#include <optional>
#include <string_view>

namespace spatial::core
{
    // Identifies how a dataset's coordinates should be interpreted. Only one
    // value exists today (matching the dataset manifest in docs/tile_format.md);
    // this is deliberately an enum rather than a free-form string so future
    // values (e.g. a georeferenced system) are a compile-time-checked addition
    // rather than a magic string threaded through the codebase.
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
