#include "spatial/data/DatasetSerializer.h"

#include <array>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace spatial::data::DatasetSerializer
{
    namespace
    {
        using nlohmann::json;

        constexpr std::array<const char*, 6> kRequiredFields = {
            "version", "name", "tileSize", "worldSize", "maxLOD", "coordinateSystem",
        };
    }

    Expected<DatasetManifest> loadManifest(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            return Error{ErrorCode::DatasetNotFound, "File not found: " + path.string()};
        }

        std::ostringstream buffer;
        buffer << in.rdbuf();

        const json root = json::parse(buffer.str(), /*callback*/ nullptr, /*allow_exceptions*/ false);
        if (root.is_discarded() || !root.is_object())
        {
            return Error{ErrorCode::InvalidDataset, "Malformed JSON: " + path.string()};
        }

        for (const char* field : kRequiredFields)
        {
            if (!root.contains(field))
            {
                return Error{ErrorCode::InvalidDataset, std::string("Missing required field \"") + field + "\": " + path.string()};
            }
        }

        DatasetManifest manifest{};

        if (!root.at("version").is_number_unsigned())
        {
            return Error{ErrorCode::InvalidDataset, "\"version\" must be a non-negative integer: " + path.string()};
        }
        manifest.version = root.at("version").get<std::uint32_t>();
        if (manifest.version == 0 || manifest.version > kDatasetManifestVersion)
        {
            return Error{ErrorCode::UnsupportedVersion, "Dataset manifest version " + std::to_string(manifest.version) +
                                                              " is not supported (max " +
                                                              std::to_string(kDatasetManifestVersion) + "): " + path.string()};
        }

        if (!root.at("name").is_string())
        {
            return Error{ErrorCode::InvalidDataset, "\"name\" must be a string: " + path.string()};
        }
        manifest.name = root.at("name").get<std::string>();

        if (!root.at("tileSize").is_number() || !root.at("worldSize").is_number())
        {
            return Error{ErrorCode::InvalidDataset, "\"tileSize\"/\"worldSize\" must be numbers: " + path.string()};
        }
        manifest.tileSize = root.at("tileSize").get<float>();
        manifest.worldSize = root.at("worldSize").get<float>();

        if (!root.at("maxLOD").is_number_unsigned())
        {
            return Error{ErrorCode::InvalidDataset, "\"maxLOD\" must be a non-negative integer: " + path.string()};
        }
        manifest.maxLOD = root.at("maxLOD").get<std::uint32_t>();

        if (!root.at("coordinateSystem").is_string())
        {
            return Error{ErrorCode::InvalidDataset, "\"coordinateSystem\" must be a string: " + path.string()};
        }
        const auto coordinateSystem = core::coordinateSystemFromString(root.at("coordinateSystem").get<std::string>());
        if (!coordinateSystem.has_value())
        {
            return Error{ErrorCode::InvalidDataset, "Unrecognized \"coordinateSystem\": " + path.string()};
        }
        manifest.coordinateSystem = *coordinateSystem;

        if (root.contains("metadata") && root.at("metadata").is_object())
        {
            for (const auto& [key, value] : root.at("metadata").items())
            {
                if (value.is_string())
                {
                    manifest.metadata.set(key, value.get<std::string>());
                }
            }
        }

        return manifest;
    }

    Expected<void> saveManifest(const DatasetManifest& manifest, const std::filesystem::path& path)
    {
        json root;
        root["version"] = manifest.version;
        root["name"] = manifest.name;
        root["tileSize"] = manifest.tileSize;
        root["worldSize"] = manifest.worldSize;
        root["maxLOD"] = manifest.maxLOD;
        root["coordinateSystem"] = std::string(core::toString(manifest.coordinateSystem));

        json metadataObject = json::object();
        for (const auto& [key, value] : manifest.metadata.entries())
        {
            metadataObject[key] = value;
        }
        root["metadata"] = metadataObject;

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return Error{ErrorCode::InvalidDataset, "Could not open for writing: " + path.string()};
        }

        out << root.dump(4);
        if (!out.good())
        {
            return Error{ErrorCode::InvalidDataset, "Write failed: " + path.string()};
        }
        return {};
    }
}
