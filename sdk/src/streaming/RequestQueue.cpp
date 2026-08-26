#include "spatial/streaming/RequestQueue.h"

namespace spatial::streaming
{
    RequestQueue::~RequestQueue() { shutdown(); }

    void RequestQueue::push(TileRequest request)
    {
        std::lock_guard lock(m_mutex);
        if (!m_pending.insert(request.id).second)
        {
            return; // already pending
        }
        m_queue.push(request);
        m_cv.notify_one();
    }

    std::optional<TileRequest> RequestQueue::pop()
    {
        std::unique_lock lock(m_mutex);
        for (;;)
        {
            m_cv.wait(lock, [this] { return !m_queue.empty() || m_shutdown; });
            if (m_queue.empty())
            {
                return std::nullopt; // shut down with nothing left
            }

            const TileRequest request = m_queue.top();
            m_queue.pop();

            if (m_pending.erase(request.id) == 0)
            {
                continue; // was cancelled after being pushed
            }
            return request;
        }
    }

    void RequestQueue::cancel(const data::TileId& id)
    {
        std::lock_guard lock(m_mutex);
        m_pending.erase(id);
    }

    void RequestQueue::shutdown()
    {
        std::lock_guard lock(m_mutex);
        m_shutdown = true;
        m_cv.notify_all();
    }

    std::size_t RequestQueue::size() const
    {
        std::lock_guard lock(m_mutex);
        return m_pending.size();
    }
}
