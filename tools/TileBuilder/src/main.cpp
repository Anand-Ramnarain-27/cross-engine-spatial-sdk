// SpatialTileBuilder — generates a dataset manifest + binary tiles
// (see docs/tile_format.md) from a procedural city.

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "spatial/core/AABB.h"
#include "spatial/data/DatasetManifest.h"
#include "spatial/data/DatasetSerializer.h"
#include "spatial/data/Mesh.h"
#include "spatial/data/TileSerializer.h"

#include "ProceduralCity.h"

namespace
{
    using namespace spatial;

    struct Options
    {
        std::filesystem::path outputDir;
        std::string datasetName = "ProceduralCity";
        std::uint32_t gridSize = 4;              // tiles per side; must be a power of two
        float tileSize = 100.0f;                 // world-space size of one tile, in meters
        std::uint32_t maxLOD = 3;
        std::uint32_t buildingsPerTileSide = 3;
        float minBuildingHeight = 5.0f;
        float maxBuildingHeight = 60.0f;
        std::uint32_t seed = 1;
    };

    void printUsage()
    {
        std::cout <<
            "SpatialTileBuilder - generates a procedural city dataset in the SDK's tile format\n\n"
            "Usage:\n"
            "  SpatialTileBuilder --output <dir> [options]\n\n"
            "Options:\n"
            "  --output <dir>              Output directory (required). Writes <dir>/<name>.world\n"
            "                               and <dir>/tiles/L{level}_{x}_{y}.tile\n"
            "  --name <name>                Dataset name (default: ProceduralCity)\n"
            "  --grid <N>                   Tiles per side, must be a power of two (default: 4)\n"
            "  --tile-size <meters>          World-space size of one tile (default: 100)\n"
            "  --max-lod <n>                 Highest LOD index generated per tile (default: 3)\n"
            "  --buildings-per-tile <n>      Buildings per tile side, i.e. an n x n grid (default: 3)\n"
            "  --min-building-height <m>     (default: 5)\n"
            "  --max-building-height <m>     (default: 60)\n"
            "  --seed <n>                    RNG seed for reproducible layout (default: 1)\n"
            "  --help                        Show this message\n";
    }

    [[nodiscard]] bool isPowerOfTwo(std::uint32_t v) noexcept { return v > 0 && (v & (v - 1)) == 0; }

    [[nodiscard]] std::uint32_t log2PowerOfTwo(std::uint32_t v) noexcept
    {
        std::uint32_t level = 0;
        while (v > 1)
        {
            v >>= 1;
            ++level;
        }
        return level;
    }

    template <typename T>
    [[nodiscard]] std::optional<T> parseNumber(std::string_view text)
    {
        T value{};
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || ptr != text.data() + text.size())
        {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<Options> parseArgs(int argc, char** argv, bool& helpRequested)
    {
        Options options{};
        helpRequested = false;

        auto next = [&](int& i) -> std::optional<std::string_view> {
            if (i + 1 >= argc)
            {
                return std::nullopt;
            }
            return std::string_view{argv[++i]};
        };

        bool outputSet = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                helpRequested = true;
                return std::nullopt;
            }
            if (arg == "--output")
            {
                const auto value = next(i);
                if (!value) { std::cerr << "Missing value for --output\n"; return std::nullopt; }
                options.outputDir = *value;
                outputSet = true;
            }
            else if (arg == "--name")
            {
                const auto value = next(i);
                if (!value) { std::cerr << "Missing value for --name\n"; return std::nullopt; }
                options.datasetName = *value;
            }
            else if (arg == "--grid")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<std::uint32_t>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --grid\n"; return std::nullopt; }
                options.gridSize = *parsed;
            }
            else if (arg == "--tile-size")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<float>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --tile-size\n"; return std::nullopt; }
                options.tileSize = *parsed;
            }
            else if (arg == "--max-lod")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<std::uint32_t>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --max-lod\n"; return std::nullopt; }
                options.maxLOD = *parsed;
            }
            else if (arg == "--buildings-per-tile")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<std::uint32_t>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --buildings-per-tile\n"; return std::nullopt; }
                options.buildingsPerTileSide = *parsed;
            }
            else if (arg == "--min-building-height")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<float>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --min-building-height\n"; return std::nullopt; }
                options.minBuildingHeight = *parsed;
            }
            else if (arg == "--max-building-height")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<float>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --max-building-height\n"; return std::nullopt; }
                options.maxBuildingHeight = *parsed;
            }
            else if (arg == "--seed")
            {
                const auto value = next(i);
                const auto parsed = value ? parseNumber<std::uint32_t>(*value) : std::nullopt;
                if (!parsed) { std::cerr << "Invalid value for --seed\n"; return std::nullopt; }
                options.seed = *parsed;
            }
            else
            {
                std::cerr << "Unrecognized argument: " << arg << "\n";
                return std::nullopt;
            }
        }

        if (!outputSet)
        {
            std::cerr << "--output is required\n";
            return std::nullopt;
        }
        if (!isPowerOfTwo(options.gridSize))
        {
            std::cerr << "--grid must be a power of two (got " << options.gridSize << ")\n";
            return std::nullopt;
        }
        if (options.tileSize <= 0.0f)
        {
            std::cerr << "--tile-size must be positive\n";
            return std::nullopt;
        }

        return options;
    }
}

int main(int argc, char** argv)
{
    bool helpRequested = false;
    const std::optional<Options> parsed = parseArgs(argc, argv, helpRequested);

    if (helpRequested)
    {
        printUsage();
        return 0;
    }
    if (!parsed)
    {
        printUsage();
        return 1;
    }
    const Options& options = *parsed;

    const std::uint32_t level = log2PowerOfTwo(options.gridSize);
    const float worldSize = options.tileSize * static_cast<float>(options.gridSize);
    const float worldMinX = -worldSize * 0.5f;
    const float worldMinZ = -worldSize * 0.5f;

    const std::filesystem::path tilesDir = options.outputDir / "tiles";
    std::error_code ec;
    std::filesystem::create_directories(tilesDir, ec);
    if (ec)
    {
        std::cerr << "Failed to create output directory " << tilesDir << ": " << ec.message() << "\n";
        return 1;
    }

    tools::ProceduralCityConfig cityConfig{};
    cityConfig.buildingsPerTileSide = options.buildingsPerTileSide;
    cityConfig.minBuildingHeight = options.minBuildingHeight;
    cityConfig.maxBuildingHeight = options.maxBuildingHeight;
    cityConfig.seed = options.seed;

    std::uint64_t totalTriangles = 0;
    std::vector<std::uint64_t> trianglesPerLOD(options.maxLOD + 1, 0);

    std::cout << "Generating " << options.gridSize << "x" << options.gridSize << " tiles (level " << level
              << "), " << (options.maxLOD + 1) << " LOD(s) each...\n";

    for (std::uint32_t y = 0; y < options.gridSize; ++y)
    {
        for (std::uint32_t x = 0; x < options.gridSize; ++x)
        {
            const data::TileId id{level, x, y};

            const float minX = worldMinX + static_cast<float>(x) * options.tileSize;
            const float minZ = worldMinZ + static_cast<float>(y) * options.tileSize;
            const core::AABB bounds{
                core::Vec3{minX, 0.0f, minZ},
                core::Vec3{minX + options.tileSize, options.maxBuildingHeight, minZ + options.tileSize},
            };

            const data::Tile tile = tools::generateProceduralTile(id, bounds, cityConfig, options.maxLOD);

            for (std::uint32_t lod = 0; lod < tile.lods().size(); ++lod)
            {
                std::uint64_t tris = 0;
                for (const data::Mesh& mesh : tile.lods()[lod].meshes)
                {
                    tris += mesh.triangleCount();
                }
                trianglesPerLOD[lod] += tris;
                if (lod == 0)
                {
                    totalTriangles += tris;
                }
            }

            const std::filesystem::path tilePath = tilesDir / (id.toString() + ".tile");
            const Expected<void> saveResult = data::TileSerializer::saveTile(tile, tilePath);
            if (!saveResult.hasValue())
            {
                std::cerr << "Failed to write " << tilePath << ": " << saveResult.error().message << "\n";
                return 1;
            }
        }
    }

    data::DatasetManifest manifest{};
    manifest.name = options.datasetName;
    manifest.tileSize = options.tileSize;
    manifest.worldSize = worldSize;
    manifest.maxLOD = options.maxLOD;
    manifest.coordinateSystem = core::CoordinateSystem::LocalCartesian;
    manifest.metadata.set("generator", "SpatialTileBuilder");
    manifest.metadata.set("tileGridLevel", std::to_string(level));
    manifest.metadata.set("tileCount", std::to_string(options.gridSize * options.gridSize));

    const std::filesystem::path manifestPath = options.outputDir / (options.datasetName + ".world");
    const Expected<void> manifestResult = data::DatasetSerializer::saveManifest(manifest, manifestPath);
    if (!manifestResult.hasValue())
    {
        std::cerr << "Failed to write " << manifestPath << ": " << manifestResult.error().message << "\n";
        return 1;
    }

    std::cout << "Wrote " << (options.gridSize * options.gridSize) << " tiles to " << tilesDir << "\n";
    std::cout << "Wrote manifest to " << manifestPath << "\n";
    std::cout << "LOD 0 triangle count: " << totalTriangles << "\n";
    for (std::uint32_t lod = 0; lod < trianglesPerLOD.size(); ++lod)
    {
        std::cout << "  LOD " << lod << ": " << trianglesPerLOD[lod] << " triangles\n";
    }

    return 0;
}
