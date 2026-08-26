#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

#include "spatial/Export.h"
#include "spatial/streaming/TileRequest.h"

namespace spatial::streaming
{
    // Thread-safe priority queue of pending tile loads. Worker threads block
    // in pop() until a request is available or the queue is shut down.
    //
    // Re-pushing an id that's already pending does not duplicate it in the
    // dispatch order (see push()); cancelling an id that's already been
    // dispatched to a worker has no effect here — see StreamingManager for
    // how in-flight cancellation is handled instead.
    class SPATIAL_API RequestQueue
    {
    public:
        RequestQueue() = default;
        ~RequestQueue();

        RequestQueue(const RequestQueue&) = delete;
        RequestQueue& operator=(const RequestQueue&) = delete;

        // Enqueues a request. If `id` is already pending, this call is a
        // no-op — the request already in the queue keeps its priority
        // (StreamingManager only calls push() once per Unloaded->Requested
        // transition, so re-priority-on-push is not needed).
        void push(TileRequest request);

        // Blocks until a request is available or shutdown() is called.
        // Returns nullopt only after shutdown, once no requests remain.
        [[nodiscard]] std::optional<TileRequest> pop();

        // Removes `id` from the queue if it hasn't been popped yet. Has no
        // effect if `id` isn't pending (e.g. already dispatched).
        void cancel(const data::TileId& id);

        void shutdown();

        [[nodiscard]] std::size_t size() const;

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::priority_queue<TileRequest> m_queue;
        std::unordered_set<data::TileId> m_pending;
        bool m_shutdown = false;
    };
}
