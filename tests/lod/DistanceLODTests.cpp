#include <catch2/catch_test_macros.hpp>

#include "spatial/lod/DistanceLOD.h"

using namespace spatial::lod;

TEST_CASE("selectLODByDistance picks the band the distance falls into", "[lod][distance]")
{
    const std::vector<float> thresholds = {100.0f, 300.0f, 800.0f};

    CHECK(selectLODByDistance(0.0f, thresholds) == 0);
    CHECK(selectLODByDistance(50.0f, thresholds) == 0);
    CHECK(selectLODByDistance(150.0f, thresholds) == 1);
    CHECK(selectLODByDistance(500.0f, thresholds) == 2);
    CHECK(selectLODByDistance(1000.0f, thresholds) == 3);
}

TEST_CASE("selectLODByDistance treats a distance exactly at a threshold as past it", "[lod][distance]")
{
    const std::vector<float> thresholds = {100.0f, 300.0f, 800.0f};

    CHECK(selectLODByDistance(100.0f, thresholds) == 1);
    CHECK(selectLODByDistance(300.0f, thresholds) == 2);
    CHECK(selectLODByDistance(800.0f, thresholds) == 3);
}

TEST_CASE("selectLODByDistance with no thresholds always returns 0", "[lod][distance]")
{
    CHECK(selectLODByDistance(9999.0f, {}) == 0);
}
