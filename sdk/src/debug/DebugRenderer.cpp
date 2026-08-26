#include "spatial/debug/DebugRenderer.h"

#include <array>

namespace spatial::debug
{
    rendering::Color colorForState(streaming::ResourceState state) noexcept
    {
        using streaming::ResourceState;
        switch (state)
        {
            case ResourceState::Resident:
                return rendering::Color{0.0f, 1.0f, 0.0f, 1.0f};
            case ResourceState::Requested:
                return rendering::Color{1.0f, 0.0f, 0.0f, 1.0f};
            case ResourceState::Loading:
            case ResourceState::LoadedCPU:
            case ResourceState::UploadPending:
                return rendering::Color{1.0f, 1.0f, 0.0f, 1.0f};
            case ResourceState::Unloaded:
            case ResourceState::UnloadRequested:
            case ResourceState::Unloading:
                return rendering::Color{0.5f, 0.5f, 0.5f, 1.0f};
        }
        return rendering::Color{1.0f, 1.0f, 1.0f, 1.0f};
    }

    void DebugRenderer::drawLine(const core::Vec3& start, const core::Vec3& end, rendering::Color color)
    {
        m_vertices.push_back(rendering::DebugVertex{start, color});
        m_vertices.push_back(rendering::DebugVertex{end, color});
    }

    void DebugRenderer::drawAABB(const core::AABB& box, rendering::Color color)
    {
        const core::Vec3& mn = box.min;
        const core::Vec3& mx = box.max;

        const std::array<core::Vec3, 8> corners = {
            core::Vec3{mn.x, mn.y, mn.z}, core::Vec3{mx.x, mn.y, mn.z},
            core::Vec3{mx.x, mx.y, mn.z}, core::Vec3{mn.x, mx.y, mn.z},
            core::Vec3{mn.x, mn.y, mx.z}, core::Vec3{mx.x, mn.y, mx.z},
            core::Vec3{mx.x, mx.y, mx.z}, core::Vec3{mn.x, mx.y, mx.z},
        };

        constexpr std::array<std::array<int, 2>, 12> edges = {{
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}, // verticals
        }};

        for (const auto& edge : edges)
        {
            drawLine(corners[static_cast<std::size_t>(edge[0])], corners[static_cast<std::size_t>(edge[1])], color);
        }
    }

    void DebugRenderer::drawTileBounds(const core::AABB& bounds, streaming::ResourceState state)
    {
        drawAABB(bounds, colorForState(state));
    }

    void DebugRenderer::flush()
    {
        if (!m_vertices.empty())
        {
            m_renderer.drawDebugLines(m_vertices);
            m_vertices.clear();
        }
    }
}
