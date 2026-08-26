#include <algorithm>
#include <numbers>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"
#include "spatial/core/Frustum.h"
#include "spatial/core/Mat4.h"
#include "spatial/core/SpatialIndex.h"
#include "spatial/core/Vec3.h"

using namespace spatial::core;

namespace
{
    AABB worldBounds() { return AABB{Vec3{-1000, -1000, -1000}, Vec3{1000, 1000, 1000}}; }

    struct RandomScene
    {
        std::vector<AABB> bounds; // bounds[i] is item i's AABB
        SpatialIndex<int> index{worldBounds()};
    };

    RandomScene makeRandomScene(std::size_t count, std::uint32_t seed)
    {
        RandomScene scene;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> posDist(-900.0f, 900.0f);
        std::uniform_real_distribution<float> sizeDist(1.0f, 20.0f);

        scene.bounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const Vec3 center{posDist(rng), posDist(rng), posDist(rng)};
            const Vec3 halfExtents{sizeDist(rng), sizeDist(rng), sizeDist(rng)};
            const AABB bounds = AABB::fromCenterExtents(center, halfExtents);
            scene.bounds.push_back(bounds);
            scene.index.insert(static_cast<int>(i), bounds);
        }
        return scene;
    }

    std::vector<int> sorted(std::vector<int> values)
    {
        std::sort(values.begin(), values.end());
        return values;
    }

    std::vector<int> bruteForceRadius(const std::vector<AABB>& bounds, const Vec3& center, float radius)
    {
        std::vector<int> result;
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            if (bounds[i].distanceSquaredToPoint(center) <= radius * radius)
            {
                result.push_back(static_cast<int>(i));
            }
        }
        return result;
    }

    std::vector<int> bruteForceFrustum(const std::vector<AABB>& bounds, const Frustum& frustum)
    {
        std::vector<int> result;
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            if (frustum.intersectsAABB(bounds[i]))
            {
                result.push_back(static_cast<int>(i));
            }
        }
        return result;
    }
}

TEST_CASE("SpatialIndex reports size after inserts", "[core][spatialindex]")
{
    SpatialIndex<int> index(worldBounds());
    CHECK(index.size() == 0);

    index.insert(1, AABB{Vec3{0, 0, 0}, Vec3{1, 1, 1}});
    index.insert(2, AABB{Vec3{5, 0, 0}, Vec3{6, 1, 1}});
    CHECK(index.size() == 2);
}

TEST_CASE("SpatialIndex queryAABB returns exactly the overlapping items", "[core][spatialindex]")
{
    SpatialIndex<int> index(worldBounds());
    index.insert(0, AABB{Vec3{0, 0, 0}, Vec3{10, 10, 10}});    // inside query region
    index.insert(1, AABB{Vec3{500, 500, 500}, Vec3{510, 510, 510}}); // far away
    index.insert(2, AABB{Vec3{-5, -5, -5}, Vec3{5, 5, 5}});    // overlaps query region

    const AABB queryRegion{Vec3{-1, -1, -1}, Vec3{20, 20, 20}};
    const std::vector<int> results = sorted(index.queryAABB(queryRegion));

    CHECK(results == std::vector<int>{0, 2});
}

TEST_CASE("SpatialIndex forces subdivision and still finds every item", "[core][spatialindex]")
{
    // maxItemsPerNode=2 forces several levels of subdivision for 50 items.
    SpatialIndex<int> index(worldBounds(), /*maxItemsPerNode*/ 2, /*maxDepth*/ 8);
    for (int i = 0; i < 50; ++i)
    {
        const auto f = static_cast<float>(i);
        index.insert(i, AABB{Vec3{f, 0, f}, Vec3{f + 0.5f, 0.5f, f + 0.5f}});
    }

    CHECK(index.size() == 50);
    const std::vector<int> all = sorted(index.queryAABB(worldBounds()));
    REQUIRE(all.size() == 50);
    for (int i = 0; i < 50; ++i)
    {
        CHECK(all[static_cast<std::size_t>(i)] == i);
    }
}

TEST_CASE("SpatialIndex handles items straddling a quadrant boundary", "[core][spatialindex]")
{
    SpatialIndex<int> index(worldBounds(), /*maxItemsPerNode*/ 1, /*maxDepth*/ 8);

    // Centered on the origin (the root's split point) so it can never fit
    // inside any single child quadrant and must stay at the root.
    const AABB straddling{Vec3{-5, -5, -5}, Vec3{5, 5, 5}};
    index.insert(0, straddling);

    for (int i = 1; i <= 10; ++i)
    {
        const auto f = static_cast<float>(i) * 50.0f;
        index.insert(i, AABB{Vec3{f, 0, f}, Vec3{f + 1, 1, f + 1}});
    }

    const std::vector<int> results = index.queryAABB(straddling);
    CHECK(std::find(results.begin(), results.end(), 0) != results.end());
}

TEST_CASE("SpatialIndex::queryRadius matches brute force over random data", "[core][spatialindex]")
{
    const RandomScene scene = makeRandomScene(500, /*seed*/ 42);

    const std::vector<Vec3> queryCenters = {
        Vec3{0, 0, 0}, Vec3{200, -100, 50}, Vec3{-500, 500, -500}, Vec3{900, 900, 900},
    };
    const std::vector<float> radii = {10.0f, 100.0f, 500.0f};

    for (const Vec3& center : queryCenters)
    {
        for (const float radius : radii)
        {
            const std::vector<int> expected = sorted(bruteForceRadius(scene.bounds, center, radius));
            const std::vector<int> actual = sorted(scene.index.queryRadius(center, radius));
            CHECK(actual == expected);
        }
    }
}

TEST_CASE("SpatialIndex::queryFrustum matches brute force over random data", "[core][spatialindex]")
{
    const RandomScene scene = makeRandomScene(500, /*seed*/ 7);

    const Mat4 view = Mat4::lookAt(Vec3{0, 200, 900}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    const Mat4 proj = Mat4::perspective(std::numbers::pi_v<float> / 3.0f, 1.5f, 1.0f, 3000.0f);
    const Frustum frustum = Frustum::fromViewProjection(proj * view);

    const std::vector<int> expected = sorted(bruteForceFrustum(scene.bounds, frustum));
    const std::vector<int> actual = sorted(scene.index.queryFrustum(frustum));
    CHECK(actual == expected);
    CHECK_FALSE(expected.empty()); // sanity: the test frustum actually sees something
}
