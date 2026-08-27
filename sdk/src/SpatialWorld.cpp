#include "spatial/SpatialWorld.h"

#include <algorithm>

#include "spatial/data/DatasetSerializer.h"

namespace spatial
{
    SpatialWorld::~SpatialWorld() { shutdown(); }

    Expected<void> SpatialWorld::loadDataset(const std::filesystem::path& manifestPath, SpatialWorldConfig config)
    {
        Expected<data::DatasetManifest> manifestResult = data::DatasetSerializer::loadManifest(manifestPath);
        if (!manifestResult.hasValue())
        {
            return manifestResult.error();
        }

        Expected<data::TileIndex> indexResult = data::TileIndex::buildUniformGrid(manifestResult.value());
        if (!indexResult.hasValue())
        {
            return indexResult.error();
        }

        // Both steps succeeded — only now do we touch existing state, so a
        // failed reload leaves a previously loaded dataset untouched.
        shutdown();

        m_config = config;
        m_manifest = std::move(manifestResult).value();
        m_tileIndex = std::move(indexResult).value();
        m_lodManager = lod::LODManager<data::TileId>(config.lod);

        const std::filesystem::path tilesDir =
            config.tilesDirectory.value_or(manifestPath.parent_path() / "tiles");
        m_streamingManager.emplace(*m_tileIndex, streaming::makeFileTileLoader(tilesDir), config.streaming);

        return {};
    }

    void SpatialWorld::shutdown()
    {
        m_gpuTiles.clear(); // releases every GPU resource via RAII
        m_debugRenderer.reset();
        m_debugRendererTarget = nullptr;
        m_streamingManager.reset();
        m_tileIndex.reset();
        m_manifest.reset();
    }

    void SpatialWorld::update(const core::CameraParams& camera, rendering::IRenderer& renderer)
    {
        m_profiler.beginFrame();

        if (!m_streamingManager.has_value())
        {
            return;
        }

        {
            const auto section = m_profiler.measure(debug::ProfileSection::StreamingUpdate);
            m_streamingManager->update(camera);
        }

        // Enqueue GPU uploads for tiles that just became resident (or
        // revive an entry mid-eviction-cleanup — see TileGPU::stillResident).
        for (const data::TileId& id : m_streamingManager->residentTileIds())
        {
            if (const auto existing = m_gpuTiles.find(id); existing != m_gpuTiles.end())
            {
                existing->second.stillResident = true;
                continue;
            }

            const data::Tile* tile = m_streamingManager->residentTile(id);
            if (tile == nullptr)
            {
                continue;
            }

            TileGPU& gpu = m_gpuTiles[id];
            gpu.lodMeshes.resize(tile->lods().size());

            std::size_t total = tile->materials().size();
            for (const data::TileLOD& lod : tile->lods())
            {
                total += lod.meshes.size();
            }
            gpu.pendingUploads = total;

            for (const data::Material& material : tile->materials())
            {
                m_uploadQueue.enqueueMaterial(material, [&gpu](rendering::MaterialResource resource) {
                    gpu.materials.push_back(std::move(resource));
                    --gpu.pendingUploads;
                });
            }
            for (std::size_t lodIndex = 0; lodIndex < tile->lods().size(); ++lodIndex)
            {
                for (const data::Mesh& mesh : tile->lods()[lodIndex].meshes)
                {
                    m_uploadQueue.enqueueMesh(mesh, [&gpu, lodIndex](rendering::MeshResource resource) {
                        gpu.lodMeshes[lodIndex].push_back(std::move(resource));
                        --gpu.pendingUploads;
                    });
                }
            }
        }

        // Mark GPU data for tiles no longer resident, but only actually
        // erase once every upload callback that captured a reference to it
        // has fired.
        for (auto& [id, gpu] : m_gpuTiles)
        {
            if (m_streamingManager->stateOf(id) != streaming::ResourceState::Resident)
            {
                gpu.stillResident = false;
            }
        }
        for (auto it = m_gpuTiles.begin(); it != m_gpuTiles.end();)
        {
            if (!it->second.stillResident && it->second.ready())
            {
                it = m_gpuTiles.erase(it);
            }
            else
            {
                ++it;
            }
        }

        {
            const auto section = m_profiler.measure(debug::ProfileSection::GPUUpload);
            m_uploadQueue.processQueue(renderer, m_config.maxGPUUploadsPerUpdate);
        }
    }

    void SpatialWorld::render(rendering::IRenderer& renderer, const core::CameraParams& camera)
    {
        if (!m_streamingManager.has_value())
        {
            m_profiler.endFrame();
            return;
        }

        {
            const auto section = m_profiler.measure(debug::ProfileSection::LODSelection);
            for (const auto& [id, gpu] : m_gpuTiles)
            {
                if (!gpu.ready() || !gpu.stillResident)
                {
                    continue;
                }
                const data::Tile* tile = m_streamingManager->residentTile(id);
                if (tile == nullptr)
                {
                    continue;
                }

                std::vector<float> geometricErrors;
                geometricErrors.reserve(tile->lods().size());
                for (const data::TileLOD& lod : tile->lods())
                {
                    geometricErrors.push_back(lod.geometricError);
                }

                const std::uint32_t rawLod = m_lodManager.selectLOD(id, tile->bounds().center(), geometricErrors, camera);
                const std::size_t lodIndex = std::min<std::size_t>(rawLod, gpu.lodMeshes.size() - 1);

                const std::vector<data::Mesh>& meshes = tile->lods()[lodIndex].meshes;
                for (std::size_t meshIndex = 0; meshIndex < gpu.lodMeshes[lodIndex].size() && meshIndex < meshes.size(); ++meshIndex)
                {
                    const int materialIndex = meshes[meshIndex].materialIndex;
                    rendering::MaterialHandle materialHandle{};
                    if (materialIndex >= 0 && static_cast<std::size_t>(materialIndex) < gpu.materials.size())
                    {
                        materialHandle = gpu.materials[static_cast<std::size_t>(materialIndex)].handle();
                    }
                    renderer.drawMesh(gpu.lodMeshes[lodIndex][meshIndex].handle(), materialHandle, core::Mat4::identity());
                }
            }
        }

        if (!m_config.debugVisualizationEnabled)
        {
            m_profiler.endFrame();
            return;
        }

        {
            const auto section = m_profiler.measure(debug::ProfileSection::DebugDraw);

            if (m_debugRendererTarget != &renderer)
            {
                m_debugRendererTarget = &renderer;
                m_debugRenderer.emplace(renderer);
            }

            for (const data::TileId& id : m_tileIndex->queryRadius(camera.position, m_config.streaming.streamingRadius))
            {
                // Prefer a resident tile's own (tightly-bounded) bounds over
                // TileIndex's generic-height bounds — see docs/architecture.md's
                // Phase 9 notes on the bug this avoids.
                std::optional<core::AABB> bounds;
                if (const data::Tile* resident = m_streamingManager->residentTile(id))
                {
                    bounds = resident->bounds();
                }
                else
                {
                    bounds = m_tileIndex->find(id);
                }

                if (bounds)
                {
                    m_debugRenderer->drawTileBounds(*bounds, m_streamingManager->stateOf(id));
                }
            }
            m_debugRenderer->flush();
        }

        m_profiler.endFrame();
    }

    streaming::StreamingStatistics SpatialWorld::statistics() const
    {
        return m_streamingManager.has_value() ? m_streamingManager->statistics() : streaming::StreamingStatistics{};
    }
}
