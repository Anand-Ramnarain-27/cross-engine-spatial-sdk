#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "spatial/rendering/ResourceHandle.h"

using namespace spatial::rendering;

TEST_CASE("Handle default-constructs invalid", "[rendering][handle]")
{
    const MeshHandle handle;
    CHECK_FALSE(handle.isValid());
    CHECK(handle.id == 0);
}

TEST_CASE("Handle with a nonzero id is valid", "[rendering][handle]")
{
    const MeshHandle handle{42};
    CHECK(handle.isValid());
}

TEST_CASE("Handle equality compares by id", "[rendering][handle]")
{
    CHECK(MeshHandle{1} == MeshHandle{1});
    CHECK_FALSE(MeshHandle{1} == MeshHandle{2});
}

TEST_CASE("Handle is usable as an unordered_map key", "[rendering][handle]")
{
    std::unordered_map<MeshHandle, int> map;
    map[MeshHandle{1}] = 10;
    map[MeshHandle{2}] = 20;

    CHECK(map[MeshHandle{1}] == 10);
    CHECK(map.size() == 2);
}
