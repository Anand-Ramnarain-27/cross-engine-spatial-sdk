#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/streaming/StreamingManager.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::streaming;

namespace
{
    // A TileLoader that blocks each call until explicitly released, and
    // records the order calls arrived in — lets tests deterministically
    // control and observe worker-thread dispatch order and in-flight state
    // instead of relying on sleep-based guessing.
    class ControllableLoader
    {
    public:
        Expected<Tile> operator()(const TileId& id)
        {
            {
                std::lock_guard lock(m_mutex);
                m_callOrder.push_back(id);
            }
            m_cv.notify_all();

            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [&] { return m_releaseAll || m_released.contains(id); });
            return Tile(id);
        }

        void release(const TileId& id)
        {
            std::lock_guard lock(m_mutex);
            m_released.insert(id);
            m_cv.notify_all();
        }

        void releaseAll()
        {
            std::lock_guard lock(m_mutex);
            m_releaseAll = true;
            m_cv.notify_all();
        }

        [[nodiscard]] std::vector<TileId> callOrder() const
        {
            std::lock_guard lock(m_mutex);
            return m_callOrder;
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::vector<TileId> m_callOrder;
        std::unordered_set<TileId> m_released;
        bool m_releaseAll = false;
    };

    template <typename Predicate>
    bool waitUntil(Predicate predicate, int timeoutMs = 2000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return predicate();
    }

    TileLoader immediateSuccessLoader()
    {
        return [](const TileId& id) -> Expected<Tile> { return Tile(id); };
    }

    // Three tiles at increasing distance from the origin along +X, all
    // within a streaming radius of 150; one tile far outside it.
    TileIndex makeTestIndex()
    {
        TileIndex index(AABB{Vec3{-100000, -1000, -1000}, Vec3{100000, 1000, 1000}});
        index.insert(TileId{0, 0, 0}, AABB{Vec3{-5, -5, -5}, Vec3{5, 5, 5}});
        index.insert(TileId{0, 1, 0}, AABB{Vec3{45, -5, -5}, Vec3{55, 5, 5}});
        index.insert(TileId{0, 2, 0}, AABB{Vec3{95, -5, -5}, Vec3{105, 5, 5}});
        index.insert(TileId{0, 3, 0}, AABB{Vec3{995, -5, -5}, Vec3{1005, 5, 5}});
        return index;
    }

    StreamingConfig testConfig(float radius = 150.0f, std::uint32_t threads = 2)
    {
        StreamingConfig config{};
        config.streamingRadius = radius;
        config.workerThreadCount = threads;
        config.maxNewRequestsPerUpdate = 16;
        return config;
    }
}

TEST_CASE("StreamingManager loads tiles within radius and leaves distant tiles unloaded", "[streaming][manager]")
{
    const TileIndex index = makeTestIndex();
    StreamingManager manager(index, immediateSuccessLoader(), testConfig());

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    const bool allResident = waitUntil([&] {
        manager.update(camera);
        return manager.stateOf(TileId{0, 0, 0}) == ResourceState::Resident &&
               manager.stateOf(TileId{0, 1, 0}) == ResourceState::Resident &&
               manager.stateOf(TileId{0, 2, 0}) == ResourceState::Resident;
    });

    CHECK(allResident);
    CHECK(manager.stateOf(TileId{0, 3, 0}) == ResourceState::Unloaded);
    REQUIRE(manager.residentTile(TileId{0, 0, 0}) != nullptr);
    CHECK(manager.residentTile(TileId{0, 0, 0})->id() == (TileId{0, 0, 0}));
    CHECK(manager.residentTile(TileId{0, 3, 0}) == nullptr);
}

TEST_CASE("StreamingManager unloads a tile once it leaves the streaming radius", "[streaming][manager]")
{
    const TileIndex index = makeTestIndex();
    StreamingManager manager(index, immediateSuccessLoader(), testConfig());

    CameraParams nearCamera{};
    nearCamera.position = Vec3{0, 0, 0};
    REQUIRE(waitUntil([&] {
        manager.update(nearCamera);
        return manager.stateOf(TileId{0, 2, 0}) == ResourceState::Resident;
    }));

    CameraParams farCamera{};
    farCamera.position = Vec3{50000, 0, 0};
    manager.update(farCamera);

    CHECK(manager.stateOf(TileId{0, 2, 0}) == ResourceState::Unloaded);
    CHECK(manager.statistics().totalUnloads >= 1);
}

TEST_CASE("StreamingManager dispatches requests nearest-first when worker capacity is limited", "[streaming][manager]")
{
    const TileIndex index = makeTestIndex();
    ControllableLoader loader;
    TileLoader loaderFn = [&loader](const TileId& id) { return loader(id); };

    StreamingManager manager(index, loaderFn, testConfig(150.0f, /*threads*/ 1));

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};
    manager.update(camera); // issues requests for the 3 in-radius tiles

    REQUIRE(waitUntil([&] { return loader.callOrder().size() >= 1; }));
    CHECK(loader.callOrder()[0] == (TileId{0, 0, 0})); // nearest first
    loader.release(TileId{0, 0, 0});

    REQUIRE(waitUntil([&] { return loader.callOrder().size() >= 2; }));
    CHECK(loader.callOrder()[1] == (TileId{0, 1, 0}));
    loader.release(TileId{0, 1, 0});

    REQUIRE(waitUntil([&] { return loader.callOrder().size() >= 3; }));
    CHECK(loader.callOrder()[2] == (TileId{0, 2, 0}));

    loader.releaseAll();
}

TEST_CASE("StreamingManager cancels a not-yet-started request and discards an in-flight one", "[streaming][manager]")
{
    const TileIndex index = makeTestIndex();
    ControllableLoader loader;
    TileLoader loaderFn = [&loader](const TileId& id) { return loader(id); };

    StreamingManager manager(index, loaderFn, testConfig(150.0f, /*threads*/ 1));

    CameraParams nearCamera{};
    nearCamera.position = Vec3{0, 0, 0};
    manager.update(nearCamera); // requests all 3 in-radius tiles; the single worker starts on the nearest

    REQUIRE(waitUntil([&] { return loader.callOrder().size() >= 1; }));
    REQUIRE(loader.callOrder()[0] == (TileId{0, 0, 0}));

    // Move away before tile 1/2 ever start, and before tile 0 finishes.
    CameraParams farCamera{};
    farCamera.position = Vec3{50000, 0, 0};
    manager.update(farCamera);

    CHECK(manager.stateOf(TileId{0, 1, 0}) == ResourceState::Unloaded); // cancelled: never started
    CHECK(manager.stateOf(TileId{0, 2, 0}) == ResourceState::Unloaded); // cancelled: never started
    CHECK(manager.stateOf(TileId{0, 0, 0}) == ResourceState::Loading);  // in flight, left alone

    loader.releaseAll();
    REQUIRE(waitUntil([&] {
        manager.update(farCamera);
        return manager.stateOf(TileId{0, 0, 0}) == ResourceState::Unloaded;
    }));

    CHECK(manager.statistics().totalLoadsCompleted == 0); // discarded, not counted as a completion
    CHECK(manager.statistics().totalCancellations >= 3);
    CHECK(loader.callOrder().size() == 1); // tile 1/2 never reached the loader at all
}

TEST_CASE("StreamingManager records a failed load without making the tile resident", "[streaming][manager]")
{
    TileIndex index(AABB{Vec3{-1000, -1000, -1000}, Vec3{1000, 1000, 1000}});
    index.insert(TileId{0, 0, 0}, AABB{Vec3{-5, -5, -5}, Vec3{5, 5, 5}});

    TileLoader failingLoader = [](const TileId&) -> Expected<Tile> {
        return Error{ErrorCode::TileLoadFailed, "simulated failure"};
    };

    StreamingManager manager(index, failingLoader, testConfig());
    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    REQUIRE(waitUntil([&] {
        manager.update(camera);
        return manager.statistics().totalLoadsFailed > 0;
    }));

    CHECK(manager.residentTile(TileId{0, 0, 0}) == nullptr);
}

TEST_CASE("StreamingManager throttles how many new requests it issues per update", "[streaming][manager]")
{
    TileIndex index(AABB{Vec3{-1000, -1000, -1000}, Vec3{1000, 1000, 1000}});
    for (int i = 0; i < 20; ++i)
    {
        const float x = static_cast<float>(i) * 2.0f;
        index.insert(TileId{0, static_cast<std::uint32_t>(i), 0}, AABB{Vec3{x - 1, -1, -1}, Vec3{x + 1, 1, 1}});
    }

    ControllableLoader loader;
    TileLoader loaderFn = [&loader](const TileId& id) { return loader(id); };

    StreamingConfig config = testConfig(150.0f, /*threads*/ 1);
    config.maxNewRequestsPerUpdate = 5;
    StreamingManager manager(index, loaderFn, config);

    CameraParams camera{};
    camera.position = Vec3{0, 0, 0};

    manager.update(camera);
    auto tracked = [&] { return manager.statistics().requestedCount + manager.statistics().loadingCount; };
    CHECK(tracked() == 5);

    manager.update(camera);
    CHECK(tracked() == 10);

    loader.releaseAll();
}
