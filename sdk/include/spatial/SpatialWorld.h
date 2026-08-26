#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

#include "spatial/Error.h"
#include "spatial/Export.h"
#include "spatial/core/CameraParams.h"
#include "spatial/data/DatasetManifest.h"
#include "spatial/data/TileIndex.h"
#include "spatial/debug/DebugRenderer.h"
#include "spatial/lod/LODManager.h"
#include "spatial/rendering/GPUResource.h"
#include "spatial/rendering/GPUUploadQueue.h"
#include "spatial/rendering/IRenderer.h"
#include "spatial/streaming/StreamingManager.h"

namespace spatial
{
    struct SpatialWorldConfig
    {
        streaming::StreamingConfig streaming;
        lod::LODConfig lod;

        // Defaults to "<manifest dir>/tiles" when unset — the convention
        // SpatialTileBuilder writes to.
        std::optional<std::filesystem::path> tilesDirectory;

        std::uint32_t maxGPUUploadsPerUpdate = 8;
        bool debugVisualizationEnabled = true;
    };

    // The SDK's top-level façade: the single object an engine integration
    // owns instead of separately wiring up StreamingManager, LODManager,
    // GPUUploadQueue, and DebugRenderer itself (which is exactly what
    // examples/StandaloneViewer/src/main.cpp did before this class
    // existed — see docs/architecture.md's Phase 9 section). Every engine
    // integration from here on talks to this class, not the modules
    // underneath it directly.
    //
    // Deliberately does not introduce `SpatialDataset`/`Camera` wrapper
    // classes beyond what already exists (`DatasetManifest`, `CameraParams`)
    // — they'd carry no behavior of their own, so per the project's "don't
    // over-engineer" rule they're not worth the extra indirection just to
    // match the brief's suggested class list exactly.
    class SPATIAL_API SpatialWorld
    {
    public:
        SpatialWorld() = default;
        ~SpatialWorld();

        SpatialWorld(const SpatialWorld&) = delete;
        SpatialWorld& operator=(const SpatialWorld&) = delete;

        // Loads a dataset manifest and (re)builds the spatial index and
        // streaming subsystem around it. Safe to call again to switch
        // datasets — on success, any previously loaded dataset and its GPU
        // resources are released first; on failure, the previous dataset
        // (if any) is left loaded and untouched.
        [[nodiscard]] Expected<void> loadDataset(const std::filesystem::path& manifestPath, SpatialWorldConfig config = {});

        // Releases the loaded dataset, its streaming state, and every GPU
        // resource currently held. `renderer` must be the same renderer
        // (or still-valid) instance those resources were created with.
        void shutdown();

        // Call once per frame: advances streaming (requests/cancels/evicts
        // tiles based on `camera`) and processes up to
        // config.maxGPUUploadsPerUpdate pending GPU uploads via `renderer`.
        void update(const core::CameraParams& camera, rendering::IRenderer& renderer);

        // Draws every GPU-ready resident tile at its currently-selected LOD,
        // plus the debug tile-bounds overlay if enabled. Call this between
        // the caller's own renderer.beginFrame(viewProjection) and
        // renderer.endFrame() — this method does not call either.
        void render(rendering::IRenderer& renderer, const core::CameraParams& camera);

        [[nodiscard]] bool isLoaded() const noexcept { return m_streamingManager.has_value(); }
        [[nodiscard]] const data::DatasetManifest& datasetManifest() const { return *m_manifest; }
        [[nodiscard]] streaming::StreamingStatistics statistics() const;

        [[nodiscard]] bool debugVisualizationEnabled() const noexcept { return m_config.debugVisualizationEnabled; }
        void setDebugVisualizationEnabled(bool enabled) noexcept { m_config.debugVisualizationEnabled = enabled; }

    private:
        // Every LOD's meshes are uploaded up front for a resident tile (the
        // tile file already contains them all — see docs/lod.md); which LOD
        // actually gets drawn each frame is chosen at draw time.
        struct TileGPU
        {
            std::vector<std::vector<rendering::MeshResource>> lodMeshes;
            std::vector<rendering::MaterialResource> materials;
            std::size_t pendingUploads = 0;
            // False once the tile is no longer resident. The entry is kept
            // alive until pendingUploads reaches 0: upload callbacks capture
            // a reference to this struct, and a tile can be evicted before
            // an upload already queued for it completes — erasing early
            // would leave that callback holding a dangling reference.
            bool stillResident = true;

            [[nodiscard]] bool ready() const noexcept { return pendingUploads == 0; }
        };

        SpatialWorldConfig m_config;
        std::optional<data::DatasetManifest> m_manifest;
        std::optional<data::TileIndex> m_tileIndex;
        std::optional<streaming::StreamingManager> m_streamingManager;
        lod::LODManager<data::TileId> m_lodManager;
        rendering::GPUUploadQueue m_uploadQueue;
        std::unordered_map<data::TileId, TileGPU> m_gpuTiles;

        rendering::IRenderer* m_debugRendererTarget = nullptr;
        std::optional<debug::DebugRenderer> m_debugRenderer;
    };
}
