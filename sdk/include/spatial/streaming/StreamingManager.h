#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "spatial/Export.h"
#include "spatial/core/CameraParams.h"
#include "spatial/data/Tile.h"
#include "spatial/data/TileIndex.h"
#include "spatial/streaming/MemoryBudget.h"
#include "spatial/streaming/ResourceState.h"
#include "spatial/streaming/TileCache.h"
#include "spatial/streaming/WorkerPool.h"

namespace spatial::streaming
{
    struct StreamingConfig
    {
        float streamingRadius = 500.0f;
        std::uint32_t workerThreadCount = 4;

        // Caps how many new load requests update() issues in one call, so a
        // large jump in the desired set (e.g. the camera teleporting) can't
        // flood the queue in a single frame; the rest follow in later frames.
        std::uint32_t maxNewRequestsPerUpdate = 16;

        float distancePriorityWeight = 0.7f;
        float directionPriorityWeight = 0.3f;

        MemoryBudgetConfig memoryBudget;
    };

    struct StreamingStatistics
    {
        std::size_t requestedCount = 0;
        std::size_t loadingCount = 0;
        std::size_t residentCount = 0;

        std::size_t cpuMemoryUsedBytes = 0;
        std::size_t gpuMemoryUsedBytes = 0;

        std::uint64_t totalLoadsCompleted = 0;
        std::uint64_t totalLoadsFailed = 0;
        std::uint64_t totalCancellations = 0;
        std::uint64_t totalUnloads = 0;
        std::uint64_t totalCacheHits = 0; // desired tiles that were already resident, no reload needed
    };

    // Builds a TileLoader that reads "<tilesDirectory>/<id>.tile" via
    // TileSerializer — the standard on-disk loader; tests typically inject
    // their own in-memory TileLoader instead.
    [[nodiscard]] SPATIAL_API TileLoader makeFileTileLoader(std::filesystem::path tilesDirectory);

    // Drives the per-tile resource lifecycle each frame: figures out which
    // tiles are desired (within streamingRadius of the camera), requests
    // missing ones, cancels in-flight work no longer needed, promotes
    // completed loads through to Resident (backed by a TileCache so a tile
    // that falls out of view and comes back doesn't need reloading from
    // disk), and evicts cached tiles once over budget. Main-thread only —
    // the worker pool it owns does the actual file I/O off-thread.
    class SPATIAL_API StreamingManager
    {
    public:
        StreamingManager(const data::TileIndex& tileIndex, TileLoader loader, StreamingConfig config = {});
        ~StreamingManager();

        StreamingManager(const StreamingManager&) = delete;
        StreamingManager& operator=(const StreamingManager&) = delete;

        void update(const core::CameraParams& camera);

        [[nodiscard]] ResourceState stateOf(const data::TileId& id) const;
        [[nodiscard]] const data::Tile* residentTile(const data::TileId& id) const;
        [[nodiscard]] std::vector<data::TileId> residentTileIds() const;

        [[nodiscard]] StreamingStatistics statistics() const;
        [[nodiscard]] const StreamingConfig& config() const noexcept { return m_config; }

    private:
        struct Entry
        {
            ResourceState state = ResourceState::Unloaded;
        };

        const data::TileIndex& m_tileIndex;
        StreamingConfig m_config;
        WorkerPool m_workerPool;
        TileCache m_cache;
        std::unordered_map<data::TileId, Entry> m_entries;
        StreamingStatistics m_stats;

        static void transition(Entry& entry, ResourceState newState);
        [[nodiscard]] float computePriority(const core::AABB& bounds, const core::CameraParams& camera) const;
    };
}
