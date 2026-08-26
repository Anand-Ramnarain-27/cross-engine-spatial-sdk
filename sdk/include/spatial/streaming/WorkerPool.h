#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "spatial/Error.h"
#include "spatial/Export.h"
#include "spatial/data/Tile.h"
#include "spatial/data/TileId.h"
#include "spatial/streaming/RequestQueue.h"

namespace spatial::streaming
{
    using TileLoader = std::function<Expected<data::Tile>(const data::TileId&)>;

    struct CompletedLoad
    {
        data::TileId id;
        Expected<data::Tile> result;
    };

    // Owns a RequestQueue and a fixed pool of threads that pull from it and
    // run `loader`. The main thread calls drainCompleted() once per frame to
    // collect finished loads — never blocks, never touches file I/O itself.
    class SPATIAL_API WorkerPool
    {
    public:
        WorkerPool(TileLoader loader, std::uint32_t threadCount);
        ~WorkerPool();

        WorkerPool(const WorkerPool&) = delete;
        WorkerPool& operator=(const WorkerPool&) = delete;

        [[nodiscard]] RequestQueue& requestQueue() noexcept { return m_queue; }

        [[nodiscard]] std::vector<CompletedLoad> drainCompleted();

        // True once a worker has popped `id` and started running the loader
        // for it, until that load completes.
        [[nodiscard]] bool isInFlight(const data::TileId& id) const;

    private:
        TileLoader m_loader;
        RequestQueue m_queue;
        std::vector<std::thread> m_threads;
        std::mutex m_completedMutex;
        std::vector<CompletedLoad> m_completed;
        mutable std::mutex m_inFlightMutex;
        std::unordered_set<data::TileId> m_inFlight;

        void workerLoop();
    };
}
