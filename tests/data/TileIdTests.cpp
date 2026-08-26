#include <unordered_set>

#include <catch2/catch_test_macros.hpp>

#include "spatial/data/TileId.h"

using namespace spatial::data;

TEST_CASE("TileId equality and toString", "[data][tileid]")
{
    CHECK(TileId{1, 2, 3} == TileId{1, 2, 3});
    CHECK_FALSE(TileId{1, 2, 3} == TileId{1, 2, 4});
    CHECK(TileId{1, 2, 3}.toString() == "L1_2_3");
}

TEST_CASE("TileId::child addresses match the quadtree scheme", "[data][tileid]")
{
    const TileId root{0, 0, 0};
    CHECK(root.child(0, 0) == TileId{1, 0, 0});
    CHECK(root.child(1, 0) == TileId{1, 1, 0});
    CHECK(root.child(0, 1) == TileId{1, 0, 1});
    CHECK(root.child(1, 1) == TileId{1, 1, 1});
}

TEST_CASE("TileId::parent is the inverse of child", "[data][tileid]")
{
    const TileId root{0, 0, 0};
    for (std::uint32_t dx = 0; dx < 2; ++dx)
    {
        for (std::uint32_t dy = 0; dy < 2; ++dy)
        {
            CHECK(root.child(dx, dy).parent() == root);
        }
    }
}

TEST_CASE("TileId is usable as an unordered_set key", "[data][tileid]")
{
    std::unordered_set<TileId> ids;
    ids.insert(TileId{0, 0, 0});
    ids.insert(TileId{1, 0, 0});
    ids.insert(TileId{1, 0, 0}); // duplicate

    CHECK(ids.size() == 2);
    CHECK(ids.contains(TileId{0, 0, 0}));
}
