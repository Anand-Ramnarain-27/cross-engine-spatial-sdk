#include "spatial/streaming/StreamingManager.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

#include "spatial/data/TileSerializer.h"

namespace spatial::streaming
{
    TileLoader makeFileTileLoader(std::filesystem::path tilesDirectory)
    {
        return [dir = std::move(tilesDirectory)](const data::TileId& id) -> Expected<data::Tile> {
            return data::TileSerializer::loadTile(dir / (id.toString() + ".tile"));
        };
    }

    StreamingManager::StreamingManager(const data::TileIndex& tileIndex, TileLoader loader, StreamingConfig config)
        : m_tileIndex(tileIndex), m_config(config), m_workerPool(std::move(loader), config.workerThreadCount)
    {
    }

    StreamingManager::~StreamingManager() = default;

    void StreamingManager::transition(Entry& entry, ResourceState newState)
    {
        assert(isValidTransition(entry.state, newState) && "invalid tile resource state transition");
        entry.state = newState;
    }

    float StreamingManager::computePriority(const core::AABB& bounds, const core::CameraParams& camera) const
    {
        const core::Vec3 tileCenter = bounds.center();
        const float dist = core::distance(tileCenter, camera.position);
        const float distanceScore = 1.0f - std::clamp(dist / m_config.streamingRadius, 0.0f, 1.0f);

        float directionScore = 0.5f;
        const core::Vec3 toTile = tileCenter - camera.position;
        const float toTileLen = toTile.length();
        if (toTileLen > 1e-4f)
        {
            const float facing = core::dot(toTile / toTileLen, camera.forward); // [-1, 1]
            directionScore = (facing + 1.0f) * 0.5f;
        }

        return m_config.distancePriorityWeight * distanceScore + m_config.directionPriorityWeight * directionScore;
    }

    void StreamingManager::update(const core::CameraParams& camera)
    {
        const std::vector<data::TileId> desired = m_tileIndex.queryRadius(camera.position, m_config.streamingRadius);
        const std::unordered_set<data::TileId> desiredSet(desired.begin(), desired.end());

        // Completed loads first, so newly-freed worker capacity is visible
        // to the new-request pass below.
        for (CompletedLoad& completed : m_workerPool.drainCompleted())
        {
            const auto it = m_entries.find(completed.id);
            if (it == m_entries.end())
            {
                continue; // cancelled and forgotten before this arrived
            }
            Entry& entry = it->second;

            // The worker may have finished before update() observed it as
            // in-flight (see the promotion pass below); catch the bookkeeping up.
            if (entry.state == ResourceState::Requested)
            {
                transition(entry, ResourceState::Loading);
            }
            if (entry.state != ResourceState::Loading)
            {
                continue; // stale duplicate result, ignore
            }

            if (!desiredSet.contains(completed.id))
            {
                transition(entry, ResourceState::Unloaded);
                m_entries.erase(it);
                ++m_stats.totalCancellations;
                continue;
            }

            if (completed.result.hasValue())
            {
                entry.tile = std::move(completed.result).value();
                transition(entry, ResourceState::LoadedCPU);
                transition(entry, ResourceState::UploadPending);
                transition(entry, ResourceState::Resident); // Phase 8 inserts a real GPU upload here
                ++m_stats.totalLoadsCompleted;
            }
            else
            {
                transition(entry, ResourceState::Unloaded);
                m_entries.erase(it);
                ++m_stats.totalLoadsFailed;
            }
        }

        // Promote Requested -> Loading for anything a worker has picked up,
        // so the cancellation pass below can tell "still cheap to cancel"
        // (Requested) apart from "already in flight" (Loading).
        for (auto& [id, entry] : m_entries)
        {
            if (entry.state == ResourceState::Requested && m_workerPool.isInFlight(id))
            {
                transition(entry, ResourceState::Loading);
            }
        }

        // Cancel or unload anything no longer desired.
        for (auto it = m_entries.begin(); it != m_entries.end();)
        {
            if (desiredSet.contains(it->first))
            {
                ++it;
                continue;
            }

            Entry& entry = it->second;
            if (entry.state == ResourceState::Requested)
            {
                m_workerPool.requestQueue().cancel(it->first);
                transition(entry, ResourceState::Unloaded);
                ++m_stats.totalCancellations;
                it = m_entries.erase(it);
            }
            else if (entry.state == ResourceState::Resident)
            {
                transition(entry, ResourceState::UnloadRequested);
                transition(entry, ResourceState::Unloading);
                transition(entry, ResourceState::Unloaded);
                ++m_stats.totalUnloads;
                it = m_entries.erase(it);
            }
            else
            {
                // Loading: can't interrupt in-flight work; it will be
                // discarded on arrival by the completed-loads pass above.
                ++it;
            }
        }

        // Issue new requests for newly-desired tiles, throttled per call.
        std::uint32_t issued = 0;
        for (const data::TileId& id : desired)
        {
            if (issued >= m_config.maxNewRequestsPerUpdate)
            {
                break;
            }
            if (m_entries.contains(id))
            {
                continue;
            }
            const std::optional<core::AABB> bounds = m_tileIndex.find(id);
            if (!bounds)
            {
                continue; // shouldn't happen: `desired` came from this same index
            }

            Entry entry{};
            transition(entry, ResourceState::Requested);
            m_entries.emplace(id, entry);

            m_workerPool.requestQueue().push(TileRequest{id, computePriority(*bounds, camera)});
            ++issued;
        }
    }

    ResourceState StreamingManager::stateOf(const data::TileId& id) const
    {
        const auto it = m_entries.find(id);
        return it == m_entries.end() ? ResourceState::Unloaded : it->second.state;
    }

    const data::Tile* StreamingManager::residentTile(const data::TileId& id) const
    {
        const auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.state != ResourceState::Resident)
        {
            return nullptr;
        }
        return &(*it->second.tile);
    }

    std::vector<data::TileId> StreamingManager::residentTileIds() const
    {
        std::vector<data::TileId> result;
        for (const auto& [id, entry] : m_entries)
        {
            if (entry.state == ResourceState::Resident)
            {
                result.push_back(id);
            }
        }
        return result;
    }

    StreamingStatistics StreamingManager::statistics() const
    {
        StreamingStatistics stats = m_stats;
        for (const auto& [id, entry] : m_entries)
        {
            switch (entry.state)
            {
                case ResourceState::Requested:
                    ++stats.requestedCount;
                    break;
                case ResourceState::Loading:
                    ++stats.loadingCount;
                    break;
                case ResourceState::Resident:
                    ++stats.residentCount;
                    break;
                default:
                    break;
            }
        }
        return stats;
    }
}
