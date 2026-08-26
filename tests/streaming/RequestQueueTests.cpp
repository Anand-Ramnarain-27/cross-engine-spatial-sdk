#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/streaming/RequestQueue.h"

using namespace spatial::data;
using namespace spatial::streaming;

TEST_CASE("RequestQueue pops the highest-priority request first", "[streaming][requestqueue]")
{
    RequestQueue queue;
    queue.push(TileRequest{TileId{0, 0, 0}, 1.0f});
    queue.push(TileRequest{TileId{0, 1, 0}, 5.0f});
    queue.push(TileRequest{TileId{0, 2, 0}, 3.0f});

    CHECK(queue.pop()->id == (TileId{0, 1, 0}));
    CHECK(queue.pop()->id == (TileId{0, 2, 0}));
    CHECK(queue.pop()->id == (TileId{0, 0, 0}));
}

TEST_CASE("RequestQueue::push ignores a duplicate id already pending", "[streaming][requestqueue]")
{
    RequestQueue queue;
    queue.push(TileRequest{TileId{0, 0, 0}, 1.0f});
    queue.push(TileRequest{TileId{0, 0, 0}, 99.0f}); // ignored: already pending
    CHECK(queue.size() == 1);

    const auto request = queue.pop();
    REQUIRE(request.has_value());
    CHECK(request->id == (TileId{0, 0, 0}));
}

TEST_CASE("RequestQueue::cancel removes a request before it's popped", "[streaming][requestqueue]")
{
    RequestQueue queue;
    queue.push(TileRequest{TileId{0, 0, 0}, 1.0f});
    queue.push(TileRequest{TileId{0, 1, 0}, 2.0f});
    queue.cancel(TileId{0, 1, 0});

    CHECK(queue.size() == 1);
    const auto request = queue.pop();
    REQUIRE(request.has_value());
    CHECK(request->id == (TileId{0, 0, 0}));
}

TEST_CASE("RequestQueue::cancel on an unknown id is a harmless no-op", "[streaming][requestqueue]")
{
    RequestQueue queue;
    queue.push(TileRequest{TileId{0, 0, 0}, 1.0f});
    queue.cancel(TileId{9, 9, 9});
    CHECK(queue.size() == 1);
}

TEST_CASE("RequestQueue::pop returns nullopt after shutdown once drained", "[streaming][requestqueue]")
{
    RequestQueue queue;
    queue.push(TileRequest{TileId{0, 0, 0}, 1.0f});
    queue.shutdown();

    CHECK(queue.pop().has_value()); // still drains what was pending
    CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("RequestQueue::pop unblocks a waiting thread when shut down", "[streaming][requestqueue]")
{
    RequestQueue queue;
    std::atomic<bool> returned{false};

    std::thread waiter([&] {
        const auto request = queue.pop();
        CHECK_FALSE(request.has_value());
        returned = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.shutdown();
    waiter.join();
    CHECK(returned.load());
}

TEST_CASE("RequestQueue handles concurrent pushes and pops without losing or duplicating requests", "[streaming][requestqueue]")
{
    RequestQueue queue;
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 200;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p)
    {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i)
            {
                // Level encodes the producer so every id is globally unique.
                queue.push(TileRequest{TileId{static_cast<std::uint32_t>(p), static_cast<std::uint32_t>(i), 0}, 1.0f});
            }
        });
    }

    std::mutex resultsMutex;
    std::vector<TileId> results;
    std::vector<std::thread> consumers;
    for (int c = 0; c < 3; ++c)
    {
        consumers.emplace_back([&] {
            while (true)
            {
                const auto request = queue.pop();
                if (!request)
                {
                    return;
                }
                std::lock_guard lock(resultsMutex);
                results.push_back(request->id);
            }
        });
    }

    for (std::thread& t : producers)
    {
        t.join();
    }

    // Wait for consumers to drain everything, then shut down to unblock them.
    while (true)
    {
        {
            std::lock_guard lock(resultsMutex);
            if (static_cast<int>(results.size()) >= kTotal)
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    queue.shutdown();
    for (std::thread& t : consumers)
    {
        t.join();
    }

    CHECK(results.size() == static_cast<std::size_t>(kTotal));
    const std::unordered_set<TileId> unique(results.begin(), results.end());
    CHECK(unique.size() == static_cast<std::size_t>(kTotal)); // no duplicates
}
