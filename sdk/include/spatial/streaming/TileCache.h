#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "spatial/Export.h"
#include "spatial/data/Tile.h"
#include "spatial/data/TileId.h"
#include "spatial/streaming/MemoryBudget.h"

namespace spatial::streaming
{
    // Owns CPU data for every currently-resident tile and decides what to
    // evict when over budget. Not thread-safe — called only by
    // StreamingManager, on the main thread.
    //
    // Eviction uses a combined score (see keepScore()): a tile's last-known
    // priority (distance/direction, from StreamingManager) minus a penalty
    // that grows with how many frames it's gone untouched. This folds the
    // brief's "LRU", "distance/priority", and "combined" eviction strategies
    // into one tunable formula rather than three separate policies.
    class SPATIAL_API TileCache
    {
    public:
        explicit TileCache(MemoryBudgetConfig config = {});

        // Inserts or replaces a tile's data and resets its recency.
        void put(const data::TileId& id, data::Tile tile, float priority);

        // Refreshes an already-cached tile's recency/priority without
        // touching its data (called when a resident tile is still desired).
        void touch(const data::TileId& id, float priority);

        [[nodiscard]] bool contains(const data::TileId& id) const;
        [[nodiscard]] const data::Tile* find(const data::TileId& id) const;

        // Evicts the lowest keepScore() entries not in `protectedIds` until
        // every budget (CPU bytes, GPU bytes, tile count) is satisfied, or
        // no evictable entries remain. Returns evicted ids in eviction order.
        [[nodiscard]] std::vector<data::TileId> evictToBudget(const std::unordered_set<data::TileId>& protectedIds);

        // Advances the logical clock keepScore() ages against. Call once
        // per StreamingManager::update(), not per-tile.
        void advanceFrame() noexcept { ++m_tick; }

        [[nodiscard]] std::size_t tileCount() const noexcept { return m_entries.size(); }
        [[nodiscard]] std::size_t cpuMemoryUsedBytes() const noexcept { return m_cpuMemoryUsed; }
        // Abstraction: no real GPU resources exist until Phase 8, so this
        // mirrors the CPU estimate rather than tracking anything separate.
        [[nodiscard]] std::size_t gpuMemoryUsedBytes() const noexcept { return m_cpuMemoryUsed; }

        [[nodiscard]] const MemoryBudgetConfig& config() const noexcept { return m_config; }

    private:
        struct Entry
        {
            data::Tile tile;
            std::size_t memoryBytes = 0;
            float lastPriority = 0.0f;
            std::uint64_t lastAccessTick = 0;
        };

        [[nodiscard]] bool overBudget() const noexcept;
        [[nodiscard]] float keepScore(const Entry& entry) const noexcept;

        MemoryBudgetConfig m_config;
        std::unordered_map<data::TileId, Entry> m_entries;
        std::uint64_t m_tick = 0;
        std::size_t m_cpuMemoryUsed = 0;
    };
}
