#include <random>
#include <vector>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spatial/core/AABB.h"
#include "spatial/core/Frustum.h"
#include "spatial/core/Mat4.h"
#include "spatial/core/SpatialIndex.h"
#include "spatial/core/Vec3.h"

using namespace spatial::core;

namespace
{
    // World is large relative to what the camera/radius queries below
    // actually see, matching this SDK's real use case: a big streamed
    // world where any one frame only touches a small neighborhood of it.
    struct Scene
    {
        std::vector<AABB> bounds;
        SpatialIndex<int> index{AABB{Vec3{-20000, -1000, -20000}, Vec3{20000, 1000, 20000}}};
    };

    Scene makeScene(std::size_t count)
    {
        Scene scene;
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> posDist(-19900.0f, 19900.0f);

        scene.bounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const Vec3 center{posDist(rng), 0.0f, posDist(rng)};
            const AABB bounds = AABB::fromCenterExtents(center, Vec3{5, 5, 5});
            scene.bounds.push_back(bounds);
            scene.index.insert(static_cast<int>(i), bounds);
        }
        return scene;
    }

    int bruteForceFrustumCount(const std::vector<AABB>& bounds, const Frustum& frustum)
    {
        int count = 0;
        for (const AABB& b : bounds)
        {
            if (frustum.intersectsAABB(b))
            {
                ++count;
            }
        }
        return count;
    }
}

// Not run by default `ctest`/plain invocation (Catch2's "[.]" hidden tag) —
// run explicitly with: spatial_sdk_tests.exe "[spatialindex][benchmark]"
TEST_CASE("SpatialIndex frustum query vs. brute force", "[.][spatialindex][benchmark]")
{
    const Scene scene = makeScene(10'000);

    const Mat4 view = Mat4::lookAt(Vec3{0, 200, 800}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    const Mat4 proj = Mat4::perspective(1.0f, 1.5f, 1.0f, 1200.0f);
    const Frustum frustum = Frustum::fromViewProjection(proj * view);

    BENCHMARK("brute force (10,000 items)")
    {
        return bruteForceFrustumCount(scene.bounds, frustum);
    };

    BENCHMARK("SpatialIndex::queryFrustum (10,000 items)")
    {
        return scene.index.queryFrustum(frustum).size();
    };
}

TEST_CASE("SpatialIndex radius query vs. brute force", "[.][spatialindex][benchmark]")
{
    const Scene scene = makeScene(10'000);
    const Vec3 center{0, 0, 0};
    const float radius = 200.0f;

    BENCHMARK("brute force (10,000 items)")
    {
        int count = 0;
        for (const AABB& b : scene.bounds)
        {
            if (b.distanceSquaredToPoint(center) <= radius * radius)
            {
                ++count;
            }
        }
        return count;
    };

    BENCHMARK("SpatialIndex::queryRadius (10,000 items)")
    {
        return scene.index.queryRadius(center, radius).size();
    };
}
