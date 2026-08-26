#include "spatial/streaming/WorkerPool.h"

namespace spatial::streaming
{
    WorkerPool::WorkerPool(TileLoader loader, std::uint32_t threadCount) : m_loader(std::move(loader))
    {
        m_threads.reserve(threadCount);
        for (std::uint32_t i = 0; i < threadCount; ++i)
        {
            m_threads.emplace_back([this] { workerLoop(); });
        }
    }

    WorkerPool::~WorkerPool()
    {
        m_queue.shutdown();
        for (std::thread& thread : m_threads)
        {
            thread.join();
        }
    }

    void WorkerPool::workerLoop()
    {
        for (;;)
        {
            std::optional<TileRequest> request = m_queue.pop();
            if (!request)
            {
                return; // queue shut down
            }

            {
                std::lock_guard lock(m_inFlightMutex);
                m_inFlight.insert(request->id);
            }

            Expected<data::Tile> result = m_loader(request->id);

            {
                std::lock_guard lock(m_inFlightMutex);
                m_inFlight.erase(request->id);
            }

            std::lock_guard lock(m_completedMutex);
            m_completed.push_back(CompletedLoad{request->id, std::move(result)});
        }
    }

    std::vector<CompletedLoad> WorkerPool::drainCompleted()
    {
        std::lock_guard lock(m_completedMutex);
        std::vector<CompletedLoad> result = std::move(m_completed);
        m_completed.clear();
        return result;
    }

    bool WorkerPool::isInFlight(const data::TileId& id) const
    {
        std::lock_guard lock(m_inFlightMutex);
        return m_inFlight.contains(id);
    }
}
