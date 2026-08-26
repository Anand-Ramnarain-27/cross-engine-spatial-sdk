// StandaloneViewer — the SDK's first complete demonstration: a real
// window, a real D3D11 IRenderer backend, and spatial::SpatialWorld (which
// itself wires StreamingManager + LODManager + GPUUploadQueue +
// DebugRenderer together) driving a real dataset on disk.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>

#include "spatial/SpatialWorld.h"
#include "spatial/data/DatasetSerializer.h"

#include "D3D11Renderer.h"
#include "FlyCamera.h"
#include "Win32Window.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::streaming;
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
        return options;
    }

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

    SpatialWorldConfig worldConfig{};
    worldConfig.streaming.streamingRadius = options.streamingRadius;
    worldConfig.streaming.workerThreadCount = options.workerThreads;
    worldConfig.streaming.memoryBudget.maxResidentTiles = options.maxResidentTiles;
    worldConfig.streaming.memoryBudget.cpuBudgetBytes = static_cast<std::size_t>(options.cpuBudgetMB) * 1024ull * 1024ull;
    if (!options.tilesDir.empty())
    {
        worldConfig.tilesDirectory = options.tilesDir;
    }

    // Fail fast on a bad dataset path before opening a window. The real
    // load happens again below, once `renderer` exists — see the
    // declaration-order note there for why.
    const Expected<data::DatasetManifest> preflightManifest = data::DatasetSerializer::loadManifest(options.datasetManifest);
    if (!preflightManifest.hasValue())
    {
        std::cerr << "Failed to load dataset: " << preflightManifest.error().message << "\n";
        return 1;
    }
    std::cout << "Loaded dataset \"" << preflightManifest.value().name << "\"\n";

    try
    {
        Win32Window window(L"Spatial SDK - Standalone Viewer - " + toWide(preflightManifest.value().name), options.width, options.height);
        D3D11Renderer renderer(window.handle(), options.width, options.height, options.assetsDir / "shaders");

        // `world` must be declared after `renderer`: its GPU resources hold
        // pointers into `renderer`, so it must be destroyed first — on
        // every exit path, including an exception unwinding this scope,
        // not just the normal one. Declaration order (reverse-order
        // destruction) guarantees that; an explicit shutdown() call before
        // the end of this block would not cover the exception path.
        SpatialWorld world;
        const Expected<void> loadResult = world.loadDataset(options.datasetManifest, worldConfig);
        if (!loadResult.hasValue())
        {
            std::cerr << "Failed to load dataset: " << loadResult.error().message << "\n";
            return 1;
        }

        FlyCamera camera;
        camera.position = Vec3{0.0f, world.datasetManifest().tileSize * 0.5f, world.datasetManifest().worldSize * 0.3f};

        constexpr float kFovYRadians = 1.0471975512f; // 60 degrees

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
                world.setDebugVisualizationEnabled(!world.debugVisualizationEnabled());
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
            world.update(cameraParams, renderer);

            const Mat4 view = camera.viewMatrix();
            const Mat4 proj = Mat4::perspective(
                kFovYRadians, static_cast<float>(window.width()) / static_cast<float>(window.height()), 0.5f, 6000.0f);

            renderer.clear(0.53f, 0.70f, 0.90f);
            renderer.beginFrame(proj * view);
            world.render(renderer, cameraParams);
            renderer.endFrame();
            renderer.present();

            if (now - lastStatsPrint > std::chrono::seconds(1))
            {
                lastStatsPrint = now;
                const StreamingStatistics stats = world.statistics();
                wchar_t title[256];
                swprintf_s(
                    title,
                    L"Spatial SDK Viewer - %hs - Resident %zu | Loading %zu | Requested %zu | CPU %.1f MB | Hits %llu",
                    world.datasetManifest().name.c_str(), stats.residentCount, stats.loadingCount, stats.requestedCount,
                    static_cast<double>(stats.cpuMemoryUsedBytes) / (1024.0 * 1024.0),
                    static_cast<unsigned long long>(stats.totalCacheHits));
                window.setTitle(title);
            }

            if (options.runSeconds > 0.0f &&
                std::chrono::duration<float>(now - startTime).count() > options.runSeconds)
            {
                const StreamingStatistics stats = world.statistics();
                std::cout << "--run-seconds elapsed; final stats: resident=" << stats.residentCount
                          << " loading=" << stats.loadingCount << " requested=" << stats.requestedCount
                          << " totalLoadsCompleted=" << stats.totalLoadsCompleted
                          << " totalLoadsFailed=" << stats.totalLoadsFailed
                          << " cpuMemoryUsedBytes=" << stats.cpuMemoryUsedBytes << "\n";
                break;
            }
        }
        // world (and the GPU resources it holds) is destroyed here, before
        // renderer, by declaration order — see the note above.
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
