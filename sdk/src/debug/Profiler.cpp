#include "spatial/debug/Profiler.h"

namespace spatial::debug
{
    void Profiler::beginFrame() noexcept
    {
        m_current = FrameProfile{};
        m_frameStart = std::chrono::steady_clock::now();
    }

    void Profiler::endFrame() noexcept
    {
        const auto elapsed = std::chrono::steady_clock::now() - m_frameStart;
        m_current.totalMs = std::chrono::duration<double, std::milli>(elapsed).count();
        m_lastFrame = m_current;
    }

    void Profiler::addSectionTime(ProfileSection section, double milliseconds) noexcept
    {
        m_current.sectionMs[static_cast<std::size_t>(section)] += milliseconds;
    }
}
