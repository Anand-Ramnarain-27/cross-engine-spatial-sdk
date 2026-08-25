#include "spatial/Version.h"

namespace spatial
{
    Version getVersion() noexcept
    {
        return Version{kVersionMajor, kVersionMinor, kVersionPatch};
    }

    const char* getVersionString() noexcept
    {
        return "0.1.0";
    }
}
