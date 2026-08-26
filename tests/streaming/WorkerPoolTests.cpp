#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/data/Tile.h"
#include "spatial/streaming/WorkerPool.h"

using namespace spatial;
using namespace spatial::data;
using namespace spatial::streaming;

namespace
{
    std::vector<CompletedLoad> waitForCompletions(WorkerPool& pool, std::size_t count, int timeoutMs = 2000)
    {
        std::vector<CompletedLoad> all;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (all.size() < count && std::chrono::steady_clock::now() < deadline)
        {
            std::vector<CompletedLoad> batch = pool.drainCompleted();
            for (CompletedLoad& c : batch)
            {
                all.push_back(std::move(c));
            }
            if (all.size() < count)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return all;
    }
}

TEST_CASE("WorkerPool dispatches a request and delivers the result", "[streaming][workerpool]")
{
    std::atomic<int> callCount{0};
    TileLoader loader = [&](const TileId& id) -> Expected<Tile> {
        ++callCount;
        return Tile(id);
    };

    WorkerPool pool(loader, 2);
    pool.requestQueue().push(TileRequest{TileId{0, 0, 0}, 1.0f});

    const std::vector<CompletedLoad> completed = waitForCompletions(pool, 1);
    REQUIRE(completed.size() == 1);
    CHECK(completed[0].id == (TileId{0, 0, 0}));
    REQUIRE(completed[0].result.hasValue());
    CHECK(callCount.load() == 1);
}

TEST_CASE("WorkerPool::isInFlight reflects an active load", "[streaming][workerpool]")
{
    std::promise<void> gate;
    const std::shared_future<void> gateFuture = gate.get_future().share();

    TileLoader loader = [gateFuture](const TileId& id) -> Expected<Tile> {
        gateFuture.wait();
        return Tile(id);
    };

    WorkerPool pool(loader, 1);
    const TileId id{0, 0, 0};
    CHECK_FALSE(pool.isInFlight(id));

    pool.requestQueue().push(TileRequest{id, 1.0f});

    bool becameInFlight = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (pool.isInFlight(id))
        {
            becameInFlight = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(becameInFlight);

    gate.set_value();

    const std::vector<CompletedLoad> completed = waitForCompletions(pool, 1);
    REQUIRE(completed.size() == 1);
    CHECK_FALSE(pool.isInFlight(id));
}

TEST_CASE("WorkerPool processes many requests across multiple threads exactly once each", "[streaming][workerpool]")
{
    std::atomic<int> callCount{0};
    TileLoader loader = [&](const TileId& id) -> Expected<Tile> {
        ++callCount;
        return Tile(id);
    };

    WorkerPool pool(loader, 4);
    constexpr int kCount = 200;
    for (int i = 0; i < kCount; ++i)
    {
        pool.requestQueue().push(TileRequest{TileId{0, static_cast<std::uint32_t>(i), 0}, 1.0f});
    }

    const std::vector<CompletedLoad> completed = waitForCompletions(pool, kCount);
    CHECK(completed.size() == static_cast<std::size_t>(kCount));
    CHECK(callCount.load() == kCount);
}

TEST_CASE("WorkerPool propagates a load failure through the result", "[streaming][workerpool]")
{
    TileLoader loader = [](const TileId&) -> Expected<Tile> {
        return Error{ErrorCode::TileLoadFailed, "simulated failure"};
    };

    WorkerPool pool(loader, 1);
    pool.requestQueue().push(TileRequest{TileId{0, 0, 0}, 1.0f});

    const std::vector<CompletedLoad> completed = waitForCompletions(pool, 1);
    REQUIRE(completed.size() == 1);
    REQUIRE_FALSE(completed[0].result.hasValue());
    CHECK(completed[0].result.error().code == ErrorCode::TileLoadFailed);
}
