#include <catch2/catch_test_macros.hpp>

#include "spatial/debug/DebugRenderer.h"

#include "rendering/MockRenderer.h"

using namespace spatial;
using namespace spatial::core;
using namespace spatial::debug;
using namespace spatial::streaming;
using spatial::tests::MockRenderer;

TEST_CASE("colorForState matches the project brief's legend", "[debug][debugrenderer]")
{
    CHECK(colorForState(ResourceState::Resident) == rendering::Color{0.0f, 1.0f, 0.0f, 1.0f});
    CHECK(colorForState(ResourceState::Requested) == rendering::Color{1.0f, 0.0f, 0.0f, 1.0f});
    CHECK(colorForState(ResourceState::Loading) == rendering::Color{1.0f, 1.0f, 0.0f, 1.0f});
    CHECK(colorForState(ResourceState::Unloaded) == rendering::Color{0.5f, 0.5f, 0.5f, 1.0f});
}

TEST_CASE("DebugRenderer::drawAABB accumulates 12 edges (24 vertices)", "[debug][debugrenderer]")
{
    MockRenderer renderer;
    DebugRenderer debugRenderer(renderer);

    debugRenderer.drawAABB(AABB{Vec3{0, 0, 0}, Vec3{1, 1, 1}}, rendering::Color{});
    CHECK(debugRenderer.pendingVertexCount() == 24);
}

TEST_CASE("DebugRenderer::flush submits one batch and clears the pending buffer", "[debug][debugrenderer]")
{
    MockRenderer renderer;
    DebugRenderer debugRenderer(renderer);

    debugRenderer.drawAABB(AABB{Vec3{0, 0, 0}, Vec3{1, 1, 1}}, rendering::Color{});
    debugRenderer.flush();

    CHECK(debugRenderer.pendingVertexCount() == 0);
    CHECK(renderer.debugLineBatchCount == 1);
    CHECK(renderer.debugLineVertices.size() == 24);
}

TEST_CASE("DebugRenderer::flush does nothing when nothing was drawn", "[debug][debugrenderer]")
{
    MockRenderer renderer;
    DebugRenderer debugRenderer(renderer);
    debugRenderer.flush();
    CHECK(renderer.debugLineBatchCount == 0);
}

TEST_CASE("DebugRenderer::drawTileBounds colors every vertex by resource state", "[debug][debugrenderer]")
{
    MockRenderer renderer;
    DebugRenderer debugRenderer(renderer);

    debugRenderer.drawTileBounds(AABB{Vec3{0, 0, 0}, Vec3{1, 1, 1}}, ResourceState::Resident);
    debugRenderer.flush();

    REQUIRE(renderer.debugLineVertices.size() == 24);
    for (const rendering::DebugVertex& vertex : renderer.debugLineVertices)
    {
        CHECK(vertex.color == rendering::Color{0.0f, 1.0f, 0.0f, 1.0f});
    }
}

TEST_CASE("DebugRenderer batches multiple draws into a single flush call", "[debug][debugrenderer]")
{
    MockRenderer renderer;
    DebugRenderer debugRenderer(renderer);

    debugRenderer.drawLine(Vec3{0, 0, 0}, Vec3{1, 0, 0}, rendering::Color{});
    debugRenderer.drawAABB(AABB{Vec3{0, 0, 0}, Vec3{1, 1, 1}}, rendering::Color{});
    CHECK(debugRenderer.pendingVertexCount() == 2 + 24);

    debugRenderer.flush();
    CHECK(renderer.debugLineBatchCount == 1); // one batched call, not two
    CHECK(renderer.debugLineVertices.size() == 2 + 24);
}
