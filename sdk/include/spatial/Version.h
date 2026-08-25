#pragma once

#include "spatial/Export.h"

namespace spatial
{
    inline constexpr int kVersionMajor = 0;
    inline constexpr int kVersionMinor = 1;
    inline constexpr int kVersionPatch = 0;

    struct Version
    {
        int major = kVersionMajor;
        int minor = kVersionMinor;
        int patch = kVersionPatch;

        [[nodiscard]] constexpr bool operator==(const Version&) const = default;
    };

    // Returns the version of the spatial SDK the calling code is linked against.
    [[nodiscard]] SPATIAL_API Version getVersion() noexcept;

    // Human-readable "MAJOR.MINOR.PATCH" version string, e.g. "0.1.0".
    [[nodiscard]] SPATIAL_API const char* getVersionString() noexcept;
}
