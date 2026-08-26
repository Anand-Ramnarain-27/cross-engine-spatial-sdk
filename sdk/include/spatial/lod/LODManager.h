#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "spatial/core/CameraParams.h"
#include "spatial/core/Vec3.h"
#include "spatial/lod/DistanceLOD.h"
#include "spatial/lod/ScreenSpaceError.h"

namespace spatial::lod
{
    struct LODConfig
    {
        bool useScreenSpaceError = true;
        float maxScreenSpaceErrorPx = 16.0f;

        // Used only when useScreenSpaceError is false. thresholds[i] is the
        // distance boundary between LOD i and LOD i+1.
        std::vector<float> distanceThresholds = {100.0f, 300.0f, 800.0f};

        // Fraction of a boundary's distance that must be crossed before an
        // adjacent-LOD change is accepted, so a camera sitting exactly on a
        // boundary doesn't flicker between two LODs every frame.
        float hysteresisRatio = 0.1f;
    };

    // Selects a per-tile LOD index with hysteresis, keyed by an opaque Key
    // (typically data::TileId) so this stays dependency-free of the tile
    // model, matching Core's zero-dependency rule.
    //
    // geometricErrors passed to selectLOD must be non-decreasing by index
    // (LOD 0 = finest/lowest error) — this is how TileLOD data is generated
    // and is required for both distance and screen-space-error thresholds
    // to come out ascending.
    template <typename Key>
    class LODManager
    {
    public:
        explicit LODManager(LODConfig config = {}) : m_config(std::move(config)) {}

        std::uint32_t selectLOD(
            const Key& key,
            const core::Vec3& tileCenter,
            const std::vector<float>& geometricErrors,
            const core::CameraParams& camera)
        {
            const float cameraDistance = core::distance(tileCenter, camera.position);
            const std::vector<float> thresholds = effectiveThresholds(geometricErrors, camera);
            const std::uint32_t candidate = selectLODByDistance(cameraDistance, thresholds);

            const auto it = m_currentLOD.find(key);
            if (it == m_currentLOD.end())
            {
                m_currentLOD.emplace(key, candidate);
                return candidate;
            }

            const std::uint32_t current = it->second;
            const std::uint32_t result = applyHysteresis(candidate, current, cameraDistance, thresholds);
            it->second = result;
            return result;
        }

        [[nodiscard]] std::optional<std::uint32_t> currentLOD(const Key& key) const
        {
            const auto it = m_currentLOD.find(key);
            return it == m_currentLOD.end() ? std::nullopt : std::optional<std::uint32_t>(it->second);
        }

        void forget(const Key& key) { m_currentLOD.erase(key); }
        void reset() { m_currentLOD.clear(); }

        [[nodiscard]] const LODConfig& config() const noexcept { return m_config; }

    private:
        LODConfig m_config;
        std::unordered_map<Key, std::uint32_t> m_currentLOD;

        [[nodiscard]] std::vector<float> effectiveThresholds(
            const std::vector<float>& geometricErrors,
            const core::CameraParams& camera) const
        {
            if (!m_config.useScreenSpaceError)
            {
                return m_config.distanceThresholds;
            }

            std::vector<float> thresholds;
            if (geometricErrors.size() <= 1)
            {
                return thresholds;
            }
            thresholds.reserve(geometricErrors.size() - 1);
            for (std::size_t i = 1; i < geometricErrors.size(); ++i)
            {
                thresholds.push_back(screenSpaceErrorCrossoverDistance(
                    geometricErrors[i], camera.verticalFovRadians, camera.viewportHeightPx, m_config.maxScreenSpaceErrorPx));
            }
            return thresholds;
        }

        [[nodiscard]] std::uint32_t applyHysteresis(
            std::uint32_t candidate,
            std::uint32_t current,
            float cameraDistance,
            const std::vector<float>& thresholds) const
        {
            if (candidate == current)
            {
                return current;
            }

            if (candidate == current + 1)
            {
                const float boundary = thresholds[current];
                return cameraDistance < boundary * (1.0f + m_config.hysteresisRatio) ? current : candidate;
            }
            if (current > 0 && candidate == current - 1)
            {
                const float boundary = thresholds[candidate];
                return cameraDistance > boundary * (1.0f - m_config.hysteresisRatio) ? current : candidate;
            }

            return candidate; // jump of more than one LOD: snap immediately
        }
    };
}
