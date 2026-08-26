#include "spatial/data/TileSerializer.h"

#include <array>
#include <fstream>

#include "data/BinaryIO.h"

namespace spatial::data::TileSerializer
{
    namespace
    {
        constexpr std::array<char, 4> kMagic = {'S', 'P', 'T', 'L'};

        void writeTileId(detail::BinaryWriter& writer, const TileId& id)
        {
            writer.writeU32(id.level);
            writer.writeU32(id.x);
            writer.writeU32(id.y);
        }

        [[nodiscard]] TileId readTileId(detail::BinaryReader& reader)
        {
            TileId id{};
            id.level = reader.readU32();
            id.x = reader.readU32();
            id.y = reader.readU32();
            return id;
        }

        void writeVec3(detail::BinaryWriter& writer, const core::Vec3& v)
        {
            writer.writeF32(v.x);
            writer.writeF32(v.y);
            writer.writeF32(v.z);
        }

        [[nodiscard]] core::Vec3 readVec3(detail::BinaryReader& reader)
        {
            const float x = reader.readF32();
            const float y = reader.readF32();
            const float z = reader.readF32();
            return core::Vec3{x, y, z};
        }
    }

    Expected<void> saveTile(const Tile& tile, const std::filesystem::path& path)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return Error{ErrorCode::TileLoadFailed, "Could not open for writing: " + path.string()};
        }

        detail::BinaryWriter writer(out);

        writer.writeBytes(kMagic.data(), kMagic.size());
        writer.writeU32(kTileFormatVersion);

        writeTileId(writer, tile.id());

        writer.writeU8(tile.parent().has_value() ? 1 : 0);
        if (tile.parent().has_value())
        {
            writeTileId(writer, *tile.parent());
        }

        writer.writeU32(static_cast<std::uint32_t>(tile.children().size()));
        for (const TileId& child : tile.children())
        {
            writeTileId(writer, child);
        }

        writeVec3(writer, tile.bounds().min);
        writeVec3(writer, tile.bounds().max);

        writer.writeU32(static_cast<std::uint32_t>(tile.lods().size()));
        for (const TileLOD& lod : tile.lods())
        {
            writer.writeF32(lod.geometricError);
            writer.writeU32(static_cast<std::uint32_t>(lod.meshes.size()));

            for (const Mesh& mesh : lod.meshes)
            {
                writer.writeI32(mesh.materialIndex);
                writer.writeU32(static_cast<std::uint32_t>(mesh.vertices.size()));
                writer.writeU32(static_cast<std::uint32_t>(mesh.indices.size()));

                for (const Vertex& v : mesh.vertices)
                {
                    writeVec3(writer, v.position);
                    writeVec3(writer, v.normal);
                    writer.writeF32(v.uv.x);
                    writer.writeF32(v.uv.y);
                }
                for (const std::uint32_t index : mesh.indices)
                {
                    writer.writeU32(index);
                }
            }
        }

        writer.writeU32(static_cast<std::uint32_t>(tile.materials().size()));
        for (const Material& mat : tile.materials())
        {
            writer.writeString(mat.name);
            writer.writeF32(mat.baseColorR);
            writer.writeF32(mat.baseColorG);
            writer.writeF32(mat.baseColorB);
            writer.writeF32(mat.baseColorA);
            writer.writeF32(mat.metallic);
            writer.writeF32(mat.roughness);
        }

        writer.writeU32(static_cast<std::uint32_t>(tile.metadata().entries().size()));
        for (const auto& [key, value] : tile.metadata().entries())
        {
            writer.writeString(key);
            writer.writeString(value);
        }

        if (!writer.ok())
        {
            return Error{ErrorCode::TileLoadFailed, "Write failed: " + path.string()};
        }
        return {};
    }

    Expected<Tile> loadTile(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            return Error{ErrorCode::TileLoadFailed, "File not found: " + path.string()};
        }

        detail::BinaryReader reader(in);

        std::array<char, 4> magic{};
        reader.readBytes(magic.data(), magic.size());
        if (!reader.ok() || magic != kMagic)
        {
            return Error{ErrorCode::CorruptTile, "Bad magic in: " + path.string()};
        }

        const std::uint32_t version = reader.readU32();
        if (version == 0 || version > kTileFormatVersion)
        {
            return Error{ErrorCode::UnsupportedVersion, "Tile format version " + std::to_string(version) +
                                                              " is not supported (max " +
                                                              std::to_string(kTileFormatVersion) + "): " + path.string()};
        }

        Tile tile(readTileId(reader));

        const bool hasParent = reader.readU8() != 0;
        if (hasParent)
        {
            tile.setParent(readTileId(reader));
        }

        const std::uint32_t childCount = reader.readCount();
        std::vector<TileId> children;
        children.reserve(childCount);
        for (std::uint32_t i = 0; i < childCount; ++i)
        {
            children.push_back(readTileId(reader));
        }
        tile.setChildren(std::move(children));

        const core::Vec3 boundsMin = readVec3(reader);
        const core::Vec3 boundsMax = readVec3(reader);
        tile.setBounds(core::AABB{boundsMin, boundsMax});

        const std::uint32_t lodCount = reader.readCount();
        std::vector<TileLOD> lods;
        lods.reserve(lodCount);
        for (std::uint32_t lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        {
            TileLOD lod{};
            lod.geometricError = reader.readF32();

            const std::uint32_t meshCount = reader.readCount();
            lod.meshes.reserve(meshCount);
            for (std::uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex)
            {
                Mesh mesh{};
                mesh.materialIndex = reader.readI32();

                const std::uint32_t vertexCount = reader.readCount();
                const std::uint32_t indexCount = reader.readCount();

                mesh.vertices.reserve(vertexCount);
                for (std::uint32_t v = 0; v < vertexCount; ++v)
                {
                    Vertex vertex{};
                    vertex.position = readVec3(reader);
                    vertex.normal = readVec3(reader);
                    vertex.uv.x = reader.readF32();
                    vertex.uv.y = reader.readF32();
                    mesh.vertices.push_back(vertex);
                }

                mesh.indices.reserve(indexCount);
                for (std::uint32_t i = 0; i < indexCount; ++i)
                {
                    mesh.indices.push_back(reader.readU32());
                }

                lod.meshes.push_back(std::move(mesh));
            }

            lods.push_back(std::move(lod));
        }
        tile.setLODs(std::move(lods));

        const std::uint32_t materialCount = reader.readCount();
        std::vector<Material> materials;
        materials.reserve(materialCount);
        for (std::uint32_t i = 0; i < materialCount; ++i)
        {
            Material mat{};
            mat.name = reader.readString();
            mat.baseColorR = reader.readF32();
            mat.baseColorG = reader.readF32();
            mat.baseColorB = reader.readF32();
            mat.baseColorA = reader.readF32();
            mat.metallic = reader.readF32();
            mat.roughness = reader.readF32();
            materials.push_back(std::move(mat));
        }
        tile.setMaterials(std::move(materials));

        const std::uint32_t metadataCount = reader.readCount();
        for (std::uint32_t i = 0; i < metadataCount; ++i)
        {
            std::string key = reader.readString();
            std::string value = reader.readString();
            tile.metadata().set(std::move(key), std::move(value));
        }

        if (!reader.ok())
        {
            return Error{ErrorCode::CorruptTile, "Truncated or malformed tile data: " + path.string()};
        }

        return tile;
    }
}
