#pragma once

#include <cstddef>
#include <vector>

#include "spatial/Export.h"
#include "spatial/core/AABB.h"
#include "spatial/core/Vec3.h"
#include "spatial/rendering/Color.h"
#include "spatial/rendering/DebugVertex.h"
#include "spatial/rendering/IRenderer.h"
#include "spatial/streaming/ResourceState.h"

namespace spatial::debug
{
    // Color legend from the project brief: green = resident, yellow = in
    // transit (loading/uploading), red = requested, gray = unloaded/unloading.
    [[nodiscard]] rendering::Color colorForState(streaming::ResourceState state) noexcept;

    // Accumulates debug line geometry and flushes it to an IRenderer in one
    // batched call, rather than one draw call per box.
    class SPATIAL_API DebugRenderer
    {
    public:
        explicit DebugRenderer(rendering::IRenderer& renderer) : m_renderer(renderer) {}

        void drawLine(const core::Vec3& start, const core::Vec3& end, rendering::Color color);
        void drawAABB(const core::AABB& box, rendering::Color color);
        void drawTileBounds(const core::AABB& bounds, streaming::ResourceState state);

        // Submits everything accumulated since the last flush and clears the batch.
        void flush();

        [[nodiscard]] std::size_t pendingVertexCount() const noexcept { return m_vertices.size(); }

    private:
        rendering::IRenderer& m_renderer;
        std::vector<rendering::DebugVertex> m_vertices;
    };
}
