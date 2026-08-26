#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "spatial/streaming/TileCache.h"

using namespace spatial::data;
using namespace spatial::streaming;

namespace
{
    MemoryBudgetConfig unlimitedBudget()
    {
        MemoryBudgetConfig config{};
        config.cpuBudgetBytes = std::numeric_limits<std::size_t>::max();
        config.gpuBudgetBytes = std::numeric_limits<std::size_t>::max();
        config.maxResidentTiles = std::numeric_limits<std::size_t>::max();
        return config;
    }
}

TEST_CASE("TileCache::put stores a tile retrievable via find/contains", "[streaming][tilecache]")
{
    TileCache cache(unlimitedBudget());
    CHECK_FALSE(cache.contains(TileId{0, 0, 0}));

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 0.5f);

    CHECK(cache.contains(TileId{0, 0, 0}));
    REQUIRE(cache.find(TileId{0, 0, 0}) != nullptr);
    CHECK(cache.find(TileId{0, 0, 0})->id() == (TileId{0, 0, 0}));
    CHECK(cache.tileCount() == 1);
}

TEST_CASE("TileCache tracks CPU memory usage and updates it on replace", "[streaming][tilecache]")
{
    TileCache cache(unlimitedBudget());
    CHECK(cache.cpuMemoryUsedBytes() == 0);

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    const std::size_t afterFirstPut = cache.cpuMemoryUsedBytes();
    CHECK(afterFirstPut > 0);

    // Replacing the same id shouldn't double-count.
    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    CHECK(cache.cpuMemoryUsedBytes() == afterFirstPut);
}

TEST_CASE("TileCache::evictToBudget frees the evicted tile's memory", "[streaming][tilecache]")
{
    MemoryBudgetConfig config = unlimitedBudget();
    config.maxResidentTiles = 0; // any resident tile is over budget

    TileCache cache(config);
    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    REQUIRE(cache.cpuMemoryUsedBytes() > 0);

    const auto evicted = cache.evictToBudget({});
    CHECK(evicted == std::vector<TileId>{TileId{0, 0, 0}});
    CHECK(cache.cpuMemoryUsedBytes() == 0);
    CHECK(cache.tileCount() == 0);
}

TEST_CASE("TileCache::evictToBudget respects protectedIds even when over budget", "[streaming][tilecache]")
{
    MemoryBudgetConfig config = unlimitedBudget();
    config.maxResidentTiles = 1;
    TileCache cache(config);

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    cache.put(TileId{0, 1, 0}, Tile(TileId{0, 1, 0}), 1.0f);
    REQUIRE(cache.tileCount() == 2);

    const auto evicted = cache.evictToBudget({TileId{0, 0, 0}, TileId{0, 1, 0}}); // both protected
    CHECK(evicted.empty());
    CHECK(cache.tileCount() == 2); // stayed over budget: nothing was evictable
}

TEST_CASE("TileCache::evictToBudget stops once the tile-count budget is satisfied", "[streaming][tilecache]")
{
    MemoryBudgetConfig config = unlimitedBudget();
    config.maxResidentTiles = 2;
    TileCache cache(config);

    for (std::uint32_t i = 0; i < 5; ++i)
    {
        cache.put(TileId{0, i, 0}, Tile(TileId{0, i, 0}), 1.0f);
    }

    const auto evicted = cache.evictToBudget({});
    CHECK(evicted.size() == 3);
    CHECK(cache.tileCount() == 2);
}

TEST_CASE("TileCache::evictToBudget prefers lower-priority entries first", "[streaming][tilecache]")
{
    MemoryBudgetConfig config = unlimitedBudget();
    config.maxResidentTiles = 2;
    TileCache cache(config);

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 0.9f); // high priority: keep
    cache.put(TileId{0, 1, 0}, Tile(TileId{0, 1, 0}), 0.1f); // low priority: evict first
    cache.put(TileId{0, 2, 0}, Tile(TileId{0, 2, 0}), 0.5f); // mid priority: keep over #1

    const auto evicted = cache.evictToBudget({});
    REQUIRE(evicted.size() == 1);
    CHECK(evicted[0] == (TileId{0, 1, 0}));
    CHECK(cache.contains(TileId{0, 0, 0}));
    CHECK(cache.contains(TileId{0, 2, 0}));
}

TEST_CASE("TileCache::touch refreshes recency so a stale entry is evicted before a recently-touched one", "[streaming][tilecache]")
{
    MemoryBudgetConfig config = unlimitedBudget();
    config.maxResidentTiles = 1;
    config.recencyWeight = 1.0f; // make age dominate priority for this test
    TileCache cache(config);

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 0.5f);
    cache.put(TileId{0, 1, 0}, Tile(TileId{0, 1, 0}), 0.5f); // same priority, same tick

    cache.advanceFrame();
    cache.advanceFrame();
    cache.touch(TileId{0, 1, 0}, 0.5f); // tile 1 is now fresh; tile 0 is 2 frames stale

    const auto evicted = cache.evictToBudget({});
    REQUIRE(evicted.size() == 1);
    CHECK(evicted[0] == (TileId{0, 0, 0}));
    CHECK(cache.contains(TileId{0, 1, 0}));
}

TEST_CASE("TileCache::evictToBudget enforces a CPU byte budget", "[streaming][tilecache]")
{
    TileCache probe(unlimitedBudget());
    probe.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    const std::size_t oneTileBytes = probe.cpuMemoryUsedBytes();

    MemoryBudgetConfig config = unlimitedBudget();
    config.cpuBudgetBytes = oneTileBytes + oneTileBytes / 2; // room for ~1.5 tiles
    TileCache cache(config);

    cache.put(TileId{0, 0, 0}, Tile(TileId{0, 0, 0}), 1.0f);
    cache.put(TileId{0, 1, 0}, Tile(TileId{0, 1, 0}), 0.1f);
    cache.put(TileId{0, 2, 0}, Tile(TileId{0, 2, 0}), 0.2f);

    const auto evicted = cache.evictToBudget({});
    CHECK_FALSE(evicted.empty());
    CHECK(cache.cpuMemoryUsedBytes() <= config.cpuBudgetBytes);
}
