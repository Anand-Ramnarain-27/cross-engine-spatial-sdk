#include "ProceduralCity.h"

#include <array>
#include <random>
#include <vector>

#include "spatial/data/Material.h"
#include "spatial/data/Mesh.h"
#include "spatial/data/TileLOD.h"

namespace spatial::tools
{
    namespace
    {
        using core::Vec2;
        using core::Vec3;
        using data::Mesh;
        using data::Vertex;

        // splitmix64-style combine, platform-independent unlike std::hash<T>.
        [[nodiscard]] std::uint64_t combineSeed(std::uint32_t seed, std::uint32_t x, std::uint32_t y) noexcept
        {
            std::uint64_t h = (static_cast<std::uint64_t>(seed) << 32) ^ (static_cast<std::uint64_t>(x) << 16) ^ y;
            h ^= h >> 33;
            h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return h;
        }

        void appendBox(Mesh& mesh, const Vec3& center, const Vec3& halfExtents)
        {
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());

            struct Face
            {
                Vec3 normal;
                std::array<Vec3, 4> corners; // in +normal-facing winding order
            };

            const Vec3 c = center;
            const Vec3 e = halfExtents;
            const std::array<Face, 6> faces = {
                Face{Vec3{0, 0, 1}, {Vec3{-e.x, -e.y, e.z}, Vec3{e.x, -e.y, e.z}, Vec3{e.x, e.y, e.z}, Vec3{-e.x, e.y, e.z}}},
                Face{Vec3{0, 0, -1}, {Vec3{e.x, -e.y, -e.z}, Vec3{-e.x, -e.y, -e.z}, Vec3{-e.x, e.y, -e.z}, Vec3{e.x, e.y, -e.z}}},
                Face{Vec3{1, 0, 0}, {Vec3{e.x, -e.y, e.z}, Vec3{e.x, -e.y, -e.z}, Vec3{e.x, e.y, -e.z}, Vec3{e.x, e.y, e.z}}},
                Face{Vec3{-1, 0, 0}, {Vec3{-e.x, -e.y, -e.z}, Vec3{-e.x, -e.y, e.z}, Vec3{-e.x, e.y, e.z}, Vec3{-e.x, e.y, -e.z}}},
                Face{Vec3{0, 1, 0}, {Vec3{-e.x, e.y, e.z}, Vec3{e.x, e.y, e.z}, Vec3{e.x, e.y, -e.z}, Vec3{-e.x, e.y, -e.z}}},
                Face{Vec3{0, -1, 0}, {Vec3{-e.x, -e.y, -e.z}, Vec3{e.x, -e.y, -e.z}, Vec3{e.x, -e.y, e.z}, Vec3{-e.x, -e.y, e.z}}},
            };

            static constexpr std::array<Vec2, 4> kFaceUVs = {Vec2{0, 0}, Vec2{1, 0}, Vec2{1, 1}, Vec2{0, 1}};

            for (const Face& face : faces)
            {
                for (std::size_t i = 0; i < 4; ++i)
                {
                    mesh.vertices.push_back(Vertex{c + face.corners[i], face.normal, kFaceUVs[i]});
                }
            }

            for (std::uint32_t face = 0; face < 6; ++face)
            {
                const std::uint32_t v0 = base + face * 4;
                mesh.indices.insert(mesh.indices.end(), {v0, v0 + 1, v0 + 2, v0, v0 + 2, v0 + 3});
            }
        }

        void appendGroundQuad(Mesh& mesh, const core::AABB& bounds)
        {
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            const float y = bounds.min.y;
            const Vec3 normal{0.0f, 1.0f, 0.0f};

            mesh.vertices.push_back(Vertex{Vec3{bounds.min.x, y, bounds.min.z}, normal, Vec2{0, 0}});
            mesh.vertices.push_back(Vertex{Vec3{bounds.max.x, y, bounds.min.z}, normal, Vec2{1, 0}});
            mesh.vertices.push_back(Vertex{Vec3{bounds.max.x, y, bounds.max.z}, normal, Vec2{1, 1}});
            mesh.vertices.push_back(Vertex{Vec3{bounds.min.x, y, bounds.max.z}, normal, Vec2{0, 1}});

            mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        struct BuildingInstance
        {
            Vec3 center;
            Vec3 halfExtents;
        };
    }

    data::Tile generateProceduralTile(
        const data::TileId& id,
        const core::AABB& bounds,
        const ProceduralCityConfig& config,
        std::uint32_t maxLOD)
    {
        data::Tile tile(id);
        tile.setBounds(bounds);

        data::Material ground{};
        ground.name = "Ground";
        ground.baseColorR = 0.25f;
        ground.baseColorG = 0.55f;
        ground.baseColorB = 0.28f;
        ground.roughness = 0.95f;

        data::Material building{};
        building.name = "Building";
        building.baseColorR = 0.65f;
        building.baseColorG = 0.62f;
        building.baseColorB = 0.58f;
        building.roughness = 0.7f;

        constexpr std::int32_t kGroundMaterialIndex = 0;
        constexpr std::int32_t kBuildingMaterialIndex = 1;
        tile.setMaterials({ground, building});

        std::mt19937_64 rng(combineSeed(config.seed, id.x, id.y));
        std::uniform_real_distribution<float> heightDist(config.minBuildingHeight, config.maxBuildingHeight);

        const std::uint32_t n = config.buildingsPerTileSide;
        const Vec3 size = bounds.size();
        const float cellWidth = n > 0 ? size.x / static_cast<float>(n) : size.x;
        const float cellDepth = n > 0 ? size.z / static_cast<float>(n) : size.z;

        std::vector<BuildingInstance> buildings;
        buildings.reserve(static_cast<std::size_t>(n) * n);
        for (std::uint32_t row = 0; row < n; ++row)
        {
            for (std::uint32_t col = 0; col < n; ++col)
            {
                const float centerX = bounds.min.x + cellWidth * (static_cast<float>(col) + 0.5f);
                const float centerZ = bounds.min.z + cellDepth * (static_cast<float>(row) + 0.5f);
                const float halfWidth = cellWidth * config.buildingFootprintFraction * 0.5f;
                const float halfDepth = cellDepth * config.buildingFootprintFraction * 0.5f;
                const float height = heightDist(rng);

                buildings.push_back(BuildingInstance{
                    Vec3{centerX, bounds.min.y + height * 0.5f, centerZ},
                    Vec3{halfWidth, height * 0.5f, halfDepth},
                });
            }
        }

        for (std::uint32_t lod = 0; lod <= maxLOD; ++lod)
        {
            data::TileLOD tileLOD{};
            tileLOD.geometricError = (lod == 0) ? 0.0f : bounds.size().x * 0.05f * static_cast<float>(1u << (lod - 1));

            Mesh groundMesh;
            groundMesh.materialIndex = kGroundMaterialIndex;
            appendGroundQuad(groundMesh, bounds);
            tileLOD.meshes.push_back(std::move(groundMesh));

            const std::uint32_t stride = 1u << lod;
            Mesh buildingMesh;
            buildingMesh.materialIndex = kBuildingMaterialIndex;
            for (std::size_t i = 0; i < buildings.size(); i += stride)
            {
                appendBox(buildingMesh, buildings[i].center, buildings[i].halfExtents);
            }
            if (!buildingMesh.indices.empty())
            {
                tileLOD.meshes.push_back(std::move(buildingMesh));
            }

            tile.addLOD(std::move(tileLOD));
        }

        return tile;
    }
}
