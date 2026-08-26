#include <catch2/catch_test_macros.hpp>

#include "spatial/core/CoordinateSystem.h"

using namespace spatial::core;

TEST_CASE("CoordinateSystem string round-trip", "[core][coordinatesystem]")
{
    CHECK(toString(CoordinateSystem::LocalCartesian) == "LOCAL_CARTESIAN");
    CHECK(coordinateSystemFromString("LOCAL_CARTESIAN") == CoordinateSystem::LocalCartesian);
}

TEST_CASE("CoordinateSystem rejects unknown strings", "[core][coordinatesystem]")
{
    CHECK_FALSE(coordinateSystemFromString("GEOREFERENCED").has_value());
    CHECK_FALSE(coordinateSystemFromString("").has_value());
}
