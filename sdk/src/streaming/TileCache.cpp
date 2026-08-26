#include "spatial/streaming/TileCache.h"

#include <optional>

#include "spatial/data/TileMemory.h"

namespace spatial::streaming
{
    TileCache::TileCache(MemoryBudgetConfig config) : m_config(config) {}

    void TileCache::put(const data::TileId& id, data::Tile tile, float priority)
    {
        const std::size_t bytes = data::estimateTileMemoryBytes(tile);
        const auto it = m_entries.find(id);
        if (it != m_entries.end())
        {
            m_cpuMemoryUsed -= it->second.memoryBytes;
            it->second.tile = std::move(tile);
            it->second.memoryBytes = bytes;
            it->second.lastPriority = priority;
            it->second.lastAccessTick = m_tick;
        }
        else
        {
            m_entries.emplace(id, Entry{std::move(tile), bytes, priority, m_tick});
        }
        m_cpuMemoryUsed += bytes;
    }

    void TileCache::touch(const data::TileId& id, float priority)
    {
        const auto it = m_entries.find(id);
        if (it != m_entries.end())
        {
            it->second.lastPriority = priority;
            it->second.lastAccessTick = m_tick;
        }
    }

    bool TileCache::contains(const data::TileId& id) const { return m_entries.contains(id); }

    const data::Tile* TileCache::find(const data::TileId& id) const
    {
        const auto it = m_entries.find(id);
        return it == m_entries.end() ? nullptr : &it->second.tile;
    }

    bool TileCache::overBudget() const noexcept
    {
        return m_cpuMemoryUsed > m_config.cpuBudgetBytes || gpuMemoryUsedBytes() > m_config.gpuBudgetBytes ||
               m_entries.size() > m_config.maxResidentTiles;
    }

    float TileCache::keepScore(const Entry& entry) const noexcept
    {
        const auto age = static_cast<float>(m_tick - entry.lastAccessTick);
        return entry.lastPriority - m_config.recencyWeight * age;
    }

    std::vector<data::TileId> TileCache::evictToBudget(const std::unordered_set<data::TileId>& protectedIds)
    {
        std::vector<data::TileId> evicted;
        while (overBudget())
        {
            std::optional<data::TileId> worst;
            float worstScore = 0.0f;
            for (const auto& [id, entry] : m_entries)
            {
                if (protectedIds.contains(id))
                {
                    continue;
                }
                const float score = keepScore(entry);
                if (!worst.has_value() || score < worstScore)
                {
                    worst = id;
                    worstScore = score;
                }
            }
            if (!worst.has_value())
            {
                break; // nothing left we're allowed to evict
            }

            const auto it = m_entries.find(*worst);
            m_cpuMemoryUsed -= it->second.memoryBytes;
            m_entries.erase(it);
            evicted.push_back(*worst);
        }
        return evicted;
    }
}
