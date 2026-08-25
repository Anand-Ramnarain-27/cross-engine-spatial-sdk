#include <catch2/catch_test_macros.hpp>

#include "spatial/Version.h"

TEST_CASE("SDK version is reported consistently", "[core][version]")
{
    const spatial::Version version = spatial::getVersion();

    CHECK(version.major == spatial::kVersionMajor);
    CHECK(version.minor == spatial::kVersionMinor);
    CHECK(version.patch == spatial::kVersionPatch);

    const std::string versionString = spatial::getVersionString();
    CHECK_FALSE(versionString.empty());
}
