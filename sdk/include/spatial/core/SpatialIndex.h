#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "spatial/core/AABB.h"
#include "spatial/core/Frustum.h"
#include "spatial/core/Vec3.h"

namespace spatial::core
{
    // Quadtree spatial index over axis-aligned bounds, generic over payload
    // T. Subdivides X/Z only (a "quad", not an "oct", tree); Y passes through
    // each node's region unchanged, so 3D frustum/sphere tests against a
    // node stay exact even though the split itself is 2D.
    //
    // An item is stored at the smallest node whose region fully contains its
    // bounds, so a node's own items are always spatially inside that node's
    // region — this is what makes query pruning by node region correct.
    template <typename T>
    class SpatialIndex
    {
    public:
        explicit SpatialIndex(const AABB& worldBounds, std::uint32_t maxItemsPerNode = 8, std::uint32_t maxDepth = 8)
            : m_maxItemsPerNode(maxItemsPerNode), m_maxDepth(maxDepth)
        {
            m_root = std::make_unique<Node>();
            m_root->region = worldBounds;
        }

        void insert(T value, const AABB& bounds)
        {
            insertAt(*m_root, std::move(value), bounds, 0);
            ++m_size;
        }

        [[nodiscard]] std::size_t size() const noexcept { return m_size; }

        [[nodiscard]] std::vector<T> queryFrustum(const Frustum& frustum) const
        {
            std::vector<T> results;
            queryFrustum(*m_root, frustum, results);
            return results;
        }

        [[nodiscard]] std::vector<T> queryRadius(const Vec3& center, float radius) const
        {
            std::vector<T> results;
            queryRadius(*m_root, center, radius, results);
            return results;
        }

        [[nodiscard]] std::vector<T> queryAABB(const AABB& region) const
        {
            std::vector<T> results;
            queryAABB(*m_root, region, results);
            return results;
        }

    private:
        struct Entry
        {
            T value;
            AABB bounds;
        };

        struct Node
        {
            AABB region;
            std::vector<Entry> items;
            std::array<std::unique_ptr<Node>, 4> children{};

            [[nodiscard]] bool isLeaf() const noexcept { return children[0] == nullptr; }
        };

        std::unique_ptr<Node> m_root;
        std::uint32_t m_maxItemsPerNode;
        std::uint32_t m_maxDepth;
        std::size_t m_size = 0;

        void insertAt(Node& node, T value, const AABB& bounds, std::uint32_t depth)
        {
            if (!node.isLeaf())
            {
                if (const int childIndex = childIndexFor(node, bounds); childIndex >= 0)
                {
                    insertAt(*node.children[static_cast<std::size_t>(childIndex)], std::move(value), bounds, depth + 1);
                    return;
                }
            }

            node.items.push_back(Entry{std::move(value), bounds});

            if (node.isLeaf() && node.items.size() > m_maxItemsPerNode && depth < m_maxDepth)
            {
                subdivide(node, depth);
            }
        }

        void subdivide(Node& node, std::uint32_t depth)
        {
            const Vec3 center = node.region.center();
            const Vec3 min = node.region.min;
            const Vec3 max = node.region.max;

            const std::array<AABB, 4> quadrants = {
                AABB{Vec3{min.x, min.y, min.z}, Vec3{center.x, max.y, center.z}},
                AABB{Vec3{center.x, min.y, min.z}, Vec3{max.x, max.y, center.z}},
                AABB{Vec3{min.x, min.y, center.z}, Vec3{center.x, max.y, max.z}},
                AABB{Vec3{center.x, min.y, center.z}, Vec3{max.x, max.y, max.z}},
            };

            for (std::size_t i = 0; i < 4; ++i)
            {
                node.children[i] = std::make_unique<Node>();
                node.children[i]->region = quadrants[i];
            }

            std::vector<Entry> remaining;
            for (Entry& entry : node.items)
            {
                if (const int childIndex = childIndexFor(node, entry.bounds); childIndex >= 0)
                {
                    insertAt(*node.children[static_cast<std::size_t>(childIndex)], std::move(entry.value), entry.bounds, depth + 1);
                }
                else
                {
                    remaining.push_back(std::move(entry));
                }
            }
            node.items = std::move(remaining);
        }

        [[nodiscard]] static int childIndexFor(const Node& node, const AABB& bounds)
        {
            const Vec3 center = node.region.center();
            const bool fitsMinusX = bounds.max.x <= center.x;
            const bool fitsPlusX = bounds.min.x >= center.x;
            const bool fitsMinusZ = bounds.max.z <= center.z;
            const bool fitsPlusZ = bounds.min.z >= center.z;

            if (fitsMinusX && fitsMinusZ) return 0;
            if (fitsPlusX && fitsMinusZ) return 1;
            if (fitsMinusX && fitsPlusZ) return 2;
            if (fitsPlusX && fitsPlusZ) return 3;
            return -1;
        }

        void queryFrustum(const Node& node, const Frustum& frustum, std::vector<T>& results) const
        {
            if (!frustum.intersectsAABB(node.region))
            {
                return;
            }
            for (const Entry& entry : node.items)
            {
                if (frustum.intersectsAABB(entry.bounds))
                {
                    results.push_back(entry.value);
                }
            }
            if (!node.isLeaf())
            {
                for (const auto& child : node.children)
                {
                    queryFrustum(*child, frustum, results);
                }
            }
        }

        void queryRadius(const Node& node, const Vec3& center, float radius, std::vector<T>& results) const
        {
            const float radiusSquared = radius * radius;
            if (node.region.distanceSquaredToPoint(center) > radiusSquared)
            {
                return;
            }
            for (const Entry& entry : node.items)
            {
                if (entry.bounds.distanceSquaredToPoint(center) <= radiusSquared)
                {
                    results.push_back(entry.value);
                }
            }
            if (!node.isLeaf())
            {
                for (const auto& child : node.children)
                {
                    queryRadius(*child, center, radius, results);
                }
            }
        }

        void queryAABB(const Node& node, const AABB& region, std::vector<T>& results) const
        {
            if (!node.region.intersectsAABB(region))
            {
                return;
            }
            for (const Entry& entry : node.items)
            {
                if (entry.bounds.intersectsAABB(region))
                {
                    results.push_back(entry.value);
                }
            }
            if (!node.isLeaf())
            {
                for (const auto& child : node.children)
                {
                    queryAABB(*child, region, results);
                }
            }
        }
    };
}
