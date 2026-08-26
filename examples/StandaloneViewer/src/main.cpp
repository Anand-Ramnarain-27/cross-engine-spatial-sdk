// StandaloneViewer — the SDK's first complete demonstration: a real
// window, a real D3D11 IRenderer backend, and StreamingManager + LODManager
// + DebugRenderer wired together against a real dataset on disk.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "spatial/data/DatasetSerializer.h"
#include "spatial/data/TileIndex.h"
#include "spatial/debug/DebugRenderer.h"
#include "spatial/lod/LODManager.h"
#include "spatial/rendering/GPUUploadQueue.h"
#include "spatial/streaming/StreamingManager.h"

#include "D3D11Renderer.h"
#include "FlyCamera.h"
#include "Win32Window.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::data;
using namespace spatial::streaming;
using namespace spatial::rendering;
using namespace spatial::lod;
using namespace spatial::debug;
using namespace viewer;

namespace
{
    struct Options
    {
        std::filesystem::path datasetManifest;
        std::filesystem::path tilesDir; // defaults to <manifest dir>/tiles
        std::filesystem::path assetsDir = "assets";
        int width = 1280;
        int height = 720;
        float streamingRadius = 400.0f;
        std::uint32_t maxResidentTiles = 256;
        std::uint32_t cpuBudgetMB = 512;
        std::uint32_t workerThreads = 4;
        float runSeconds = 0.0f; // 0 = run until window closed
    };

    void printUsage()
    {
        std::cout <<
            "StandaloneViewer - the Cross-Engine Spatial SDK's reference viewer\n\n"
            "Usage:\n"
            "  StandaloneViewer --dataset <path-to.world> [options]\n\n"
            "Options:\n"
            "  --dataset <path>            Dataset manifest to load (required)\n"
            "  --tiles <dir>               Tile directory (default: <manifest dir>/tiles)\n"
            "  --assets <dir>              Directory containing shaders/ (default: assets)\n"
            "  --width / --height <n>      Window size (default: 1280x720)\n"
            "  --streaming-radius <m>      (default: 400)\n"
            "  --max-resident-tiles <n>    (default: 256)\n"
            "  --cpu-budget-mb <n>         (default: 512)\n"
            "  --worker-threads <n>        (default: 4)\n"
            "  --run-seconds <s>           Auto-exit after s seconds (for automated smoke tests)\n"
            "  --help                      Show this message\n\n"
            "Controls: WASD move, Space/Ctrl up/down, hold right mouse button to look,\n"
            "F1 toggles debug tile-bounds visualization, Esc quits.\n";
    }

    template <typename T>
    std::optional<T> parseNumber(const std::string& text)
    {
        try
        {
            if constexpr (std::is_same_v<T, float>)
            {
                return std::stof(text);
            }
            else
            {
                return static_cast<T>(std::stoul(text));
            }
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<Options> parseArgs(int argc, char** argv, bool& helpRequested)
    {
        Options options{};
        helpRequested = false;
        bool datasetSet = false;

        auto next = [&](int& i) -> std::optional<std::string> {
            if (i + 1 >= argc)
            {
                return std::nullopt;
            }
            return std::string{argv[++i]};
        };

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                helpRequested = true;
                return std::nullopt;
            }
            if (arg == "--dataset")
            {
                const auto v = next(i);
                if (!v) { std::cerr << "Missing value for --dataset\n"; return std::nullopt; }
                options.datasetManifest = *v;
                datasetSet = true;
            }
            else if (arg == "--tiles")
            {
                const auto v = next(i);
                if (!v) { std::cerr << "Missing value for --tiles\n"; return std::nullopt; }
                options.tilesDir = *v;
            }
            else if (arg == "--assets")
            {
                const auto v = next(i);
                if (!v) { std::cerr << "Missing value for --assets\n"; return std::nullopt; }
                options.assetsDir = *v;
            }
            else if (arg == "--width")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<std::uint32_t>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --width\n"; return std::nullopt; }
                options.width = static_cast<int>(*parsed);
            }
            else if (arg == "--height")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<std::uint32_t>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --height\n"; return std::nullopt; }
                options.height = static_cast<int>(*parsed);
            }
            else if (arg == "--streaming-radius")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<float>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --streaming-radius\n"; return std::nullopt; }
                options.streamingRadius = *parsed;
            }
            else if (arg == "--max-resident-tiles")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<std::uint32_t>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --max-resident-tiles\n"; return std::nullopt; }
                options.maxResidentTiles = *parsed;
            }
            else if (arg == "--cpu-budget-mb")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<std::uint32_t>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --cpu-budget-mb\n"; return std::nullopt; }
                options.cpuBudgetMB = *parsed;
            }
            else if (arg == "--worker-threads")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<std::uint32_t>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --worker-threads\n"; return std::nullopt; }
                options.workerThreads = *parsed;
            }
            else if (arg == "--run-seconds")
            {
                const auto v = next(i);
                const auto parsed = v ? parseNumber<float>(*v) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --run-seconds\n"; return std::nullopt; }
                options.runSeconds = *parsed;
            }
            else
            {
                std::cerr << "Unrecognized argument: " << arg << "\n";
                return std::nullopt;
            }
        }

        if (!datasetSet)
        {
            std::cerr << "--dataset is required\n";
            return std::nullopt;
        }
        if (options.tilesDir.empty())
        {
            options.tilesDir = options.datasetManifest.parent_path() / "tiles";
        }
        return options;
    }

    // GPU-side state for one resident tile: every LOD's meshes uploaded up
    // front (the tile file already contains all of them — see
    // docs/lod.md), plus the tile's materials. Which LOD actually gets
    // drawn each frame is chosen at draw time by LODManager.
    struct TileGPU
    {
        std::vector<std::vector<MeshResource>> lodMeshes;
        std::vector<MaterialResource> materials;
        std::size_t pendingUploads = 0;
        // False once the tile is no longer resident. The map entry itself
        // is kept alive until pendingUploads reaches 0: upload callbacks
        // capture a reference to this struct, and callbacks already queued
        // can still fire after the tile stops being desired (a tile can be
        // evicted before an in-flight upload for it completes) — erasing
        // the entry early would leave those callbacks holding a dangling
        // reference.
        bool stillResident = true;

        [[nodiscard]] bool ready() const noexcept { return pendingUploads == 0; }
    };

    std::wstring toWide(const std::string& s) { return std::wstring(s.begin(), s.end()); }
}

int main(int argc, char** argv)
{
    bool helpRequested = false;
    const std::optional<Options> parsedOptions = parseArgs(argc, argv, helpRequested);
    if (helpRequested)
    {
        printUsage();
        return 0;
    }
    if (!parsedOptions)
    {
        printUsage();
        return 1;
    }
    const Options& options = *parsedOptions;

    const Expected<DatasetManifest> manifestResult = DatasetSerializer::loadManifest(options.datasetManifest);
    if (!manifestResult.hasValue())
    {
        std::cerr << "Failed to load dataset manifest: " << manifestResult.error().message << "\n";
        return 1;
    }
    const DatasetManifest& manifest = manifestResult.value();

    const Expected<TileIndex> indexResult = TileIndex::buildUniformGrid(manifest);
    if (!indexResult.hasValue())
    {
        std::cerr << "Failed to build tile index: " << indexResult.error().message << "\n";
        return 1;
    }
    const TileIndex& tileIndex = indexResult.value();

    std::cout << "Loaded dataset \"" << manifest.name << "\" (" << tileIndex.size() << " tiles, "
              << options.tilesDir << ")\n";

    try
    {
        Win32Window window(L"Spatial SDK - Standalone Viewer - " + toWide(manifest.name), options.width, options.height);
        D3D11Renderer renderer(window.handle(), options.width, options.height, options.assetsDir / "shaders");

        StreamingConfig streamConfig{};
        streamConfig.streamingRadius = options.streamingRadius;
        streamConfig.workerThreadCount = options.workerThreads;
        streamConfig.memoryBudget.maxResidentTiles = options.maxResidentTiles;
        streamConfig.memoryBudget.cpuBudgetBytes = static_cast<std::size_t>(options.cpuBudgetMB) * 1024ull * 1024ull;

        StreamingManager streamingManager(tileIndex, makeFileTileLoader(options.tilesDir), streamConfig);
        LODManager<TileId> lodManager;
        GPUUploadQueue uploadQueue;
        DebugRenderer debugRenderer(renderer);

        FlyCamera camera;
        camera.position = Vec3{0.0f, manifest.tileSize * 0.5f, manifest.worldSize * 0.3f};

        std::unordered_map<TileId, TileGPU> gpuTiles;
        bool showDebugBounds = true;

        constexpr float kFovYRadians = 1.0471975512f; // 60 degrees
        constexpr std::size_t kMaxUploadsPerFrame = 8;

        const auto startTime = std::chrono::steady_clock::now();
        auto lastFrameTime = startTime;
        auto lastStatsPrint = startTime;

        while (window.processMessages())
        {
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;

            if (window.isKeyDown(VK_ESCAPE))
            {
                break;
            }
            if (window.consumeKeyPressed(VK_F1))
            {
                showDebugBounds = !showDebugBounds;
            }

            Vec3 localMove{0, 0, 0};
            if (window.isKeyDown('W')) localMove.z += 1.0f;
            if (window.isKeyDown('S')) localMove.z -= 1.0f;
            if (window.isKeyDown('D')) localMove.x += 1.0f;
            if (window.isKeyDown('A')) localMove.x -= 1.0f;
            if (window.isKeyDown(VK_SPACE)) localMove.y += 1.0f;
            if (window.isKeyDown(VK_CONTROL)) localMove.y -= 1.0f;
            camera.move(localMove, dt);

            if (window.isMouseCaptured())
            {
                const MouseDelta delta = window.consumeMouseDelta();
                camera.look(delta.dx, delta.dy);
            }

            if (window.consumeResized())
            {
                renderer.resize(window.width(), window.height());
            }

            const CameraParams cameraParams = camera.toCameraParams(static_cast<float>(window.height()), kFovYRadians);
            streamingManager.update(cameraParams);

            // Enqueue GPU uploads for tiles that just became resident.
            for (const TileId& id : streamingManager.residentTileIds())
            {
                if (const auto existing = gpuTiles.find(id); existing != gpuTiles.end())
                {
                    // Revive an entry that was mid-eviction-cleanup (its
                    // pending uploads hadn't finished yet) rather than
                    // leaving it stuck un-rendered.
                    existing->second.stillResident = true;
                    continue;
                }
                const Tile* tile = streamingManager.residentTile(id);
                if (tile == nullptr)
                {
                    continue;
                }

                TileGPU& gpu = gpuTiles[id];
                gpu.lodMeshes.resize(tile->lods().size());

                std::size_t total = tile->materials().size();
                for (const TileLOD& lod : tile->lods())
                {
                    total += lod.meshes.size();
                }
                gpu.pendingUploads = total;

                for (const Material& material : tile->materials())
                {
                    uploadQueue.enqueueMaterial(material, [&gpu](MaterialResource resource) {
                        gpu.materials.push_back(std::move(resource));
                        --gpu.pendingUploads;
                    });
                }
                for (std::size_t lodIndex = 0; lodIndex < tile->lods().size(); ++lodIndex)
                {
                    for (const Mesh& mesh : tile->lods()[lodIndex].meshes)
                    {
                        uploadQueue.enqueueMesh(mesh, [&gpu, lodIndex](MeshResource resource) {
                            gpu.lodMeshes[lodIndex].push_back(std::move(resource));
                            --gpu.pendingUploads;
                        });
                    }
                }
            }

            // Mark GPU data for tiles the streaming manager no longer
            // considers resident, but only actually erase once every
            // upload callback that captured a reference to it has fired.
            for (auto& [id, gpu] : gpuTiles)
            {
                if (streamingManager.stateOf(id) != ResourceState::Resident)
                {
                    gpu.stillResident = false;
                }
            }
            for (auto it = gpuTiles.begin(); it != gpuTiles.end();)
            {
                if (!it->second.stillResident && it->second.ready())
                {
                    it = gpuTiles.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            uploadQueue.processQueue(renderer, kMaxUploadsPerFrame);

            const Mat4 view = camera.viewMatrix();
            const Mat4 proj = Mat4::perspective(
                kFovYRadians, static_cast<float>(window.width()) / static_cast<float>(window.height()), 0.5f, 6000.0f);
            const Mat4 viewProj = proj * view;

            renderer.clear(0.53f, 0.70f, 0.90f);
            renderer.beginFrame(viewProj);

            for (const auto& [id, gpu] : gpuTiles)
            {
                if (!gpu.ready() || !gpu.stillResident)
                {
                    continue;
                }
                const Tile* tile = streamingManager.residentTile(id);
                if (tile == nullptr)
                {
                    continue;
                }

                std::vector<float> geometricErrors;
                geometricErrors.reserve(tile->lods().size());
                for (const TileLOD& lod : tile->lods())
                {
                    geometricErrors.push_back(lod.geometricError);
                }

                const std::uint32_t rawLod = lodManager.selectLOD(id, tile->bounds().center(), geometricErrors, cameraParams);
                const std::size_t lodIndex = std::min<std::size_t>(rawLod, gpu.lodMeshes.size() - 1);

                const std::vector<Mesh>& meshes = tile->lods()[lodIndex].meshes;
                for (std::size_t meshIndex = 0; meshIndex < gpu.lodMeshes[lodIndex].size() && meshIndex < meshes.size(); ++meshIndex)
                {
                    const int materialIndex = meshes[meshIndex].materialIndex;
                    MaterialHandle materialHandle{};
                    if (materialIndex >= 0 && static_cast<std::size_t>(materialIndex) < gpu.materials.size())
                    {
                        materialHandle = gpu.materials[static_cast<std::size_t>(materialIndex)].handle();
                    }
                    renderer.drawMesh(gpu.lodMeshes[lodIndex][meshIndex].handle(), materialHandle, Mat4::identity());
                }
            }

            if (showDebugBounds)
            {
                for (const TileId& id : tileIndex.queryRadius(cameraParams.position, options.streamingRadius))
                {
                    // Prefer the resident tile's own bounds: TileIndex's
                    // bounds use the dataset manifest's generic Y range
                    // ([-1000, 1000] — see DatasetManifest::worldBounds(),
                    // there's no real per-dataset height field yet), which
                    // makes every wireframe box 2000 units tall. A loaded
                    // tile's own bounds() are tight to its actual content.
                    std::optional<AABB> bounds;
                    if (const Tile* resident = streamingManager.residentTile(id))
                    {
                        bounds = resident->bounds();
                    }
                    else
                    {
                        bounds = tileIndex.find(id);
                    }

                    if (bounds)
                    {
                        debugRenderer.drawTileBounds(*bounds, streamingManager.stateOf(id));
                    }
                }
                debugRenderer.flush();
            }

            renderer.endFrame();
            renderer.present();

            if (now - lastStatsPrint > std::chrono::seconds(1))
            {
                lastStatsPrint = now;
                const StreamingStatistics stats = streamingManager.statistics();
                wchar_t title[256];
                swprintf_s(
                    title,
                    L"Spatial SDK Viewer - %hs - Resident %zu | Loading %zu | Requested %zu | CPU %.1f MB | Hits %llu",
                    manifest.name.c_str(), stats.residentCount, stats.loadingCount, stats.requestedCount,
                    static_cast<double>(stats.cpuMemoryUsedBytes) / (1024.0 * 1024.0),
                    static_cast<unsigned long long>(stats.totalCacheHits));
                window.setTitle(title);
            }

            if (options.runSeconds > 0.0f &&
                std::chrono::duration<float>(now - startTime).count() > options.runSeconds)
            {
                const StreamingStatistics stats = streamingManager.statistics();
                std::cout << "--run-seconds elapsed; final stats: resident=" << stats.residentCount
                          << " loading=" << stats.loadingCount << " requested=" << stats.requestedCount
                          << " totalLoadsCompleted=" << stats.totalLoadsCompleted
                          << " totalLoadsFailed=" << stats.totalLoadsFailed
                          << " cpuMemoryUsedBytes=" << stats.cpuMemoryUsedBytes << "\n";
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
