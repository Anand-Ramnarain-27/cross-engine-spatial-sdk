#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "spatial/Export.h"

namespace spatial::debug
{
    // Named CPU sections timed inside SpatialWorld::update()/render(). A
    // fixed enum rather than string keys: no per-frame allocation or
    // hashing, and every valid section is known at compile time.
    enum class ProfileSection : std::uint8_t
    {
        StreamingUpdate, // StreamingManager::update() — request/cancel/unload decisions
        GPUUpload,       // GPUUploadQueue::processQueue() — mesh/material uploads this frame
        LODSelection,    // LODManager::selectLOD() calls, summed across resident tiles
        DebugDraw,       // DebugRenderer tile-bounds accumulation + flush()
        Count,
    };

    struct FrameProfile
    {
        std::array<double, static_cast<std::size_t>(ProfileSection::Count)> sectionMs{};
        double totalMs = 0.0;

        [[nodiscard]] double section(ProfileSection s) const noexcept
        {
            return sectionMs[static_cast<std::size_t>(s)];
        }
    };

    // Per-frame CPU timing. Not thread-safe — intended to be called only
    // from the thread driving SpatialWorld::update()/render(), matching
    // every other per-frame SpatialWorld state.
    class SPATIAL_API Profiler
    {
    public:
        // RAII section timer: records elapsed wall-clock time into the
        // profiler's current frame when it goes out of scope.
        class ScopedSection
        {
        public:
            ScopedSection(Profiler& profiler, ProfileSection section) noexcept
                : m_profiler(profiler), m_section(section), m_start(std::chrono::steady_clock::now())
            {
            }

            ~ScopedSection()
            {
                const auto elapsed = std::chrono::steady_clock::now() - m_start;
                m_profiler.addSectionTime(m_section, std::chrono::duration<double, std::milli>(elapsed).count());
            }

            ScopedSection(const ScopedSection&) = delete;
            ScopedSection& operator=(const ScopedSection&) = delete;

        private:
            Profiler& m_profiler;
            ProfileSection m_section;
            std::chrono::steady_clock::time_point m_start;
        };

        // Starts timing a new frame: the previous frame's totals become
        // available via lastFrame(), and the current frame's section
        // times reset to zero.
        void beginFrame() noexcept;

        // Finalizes the current frame's total time.
        void endFrame() noexcept;

        [[nodiscard]] ScopedSection measure(ProfileSection section) noexcept
        {
            return ScopedSection(*this, section);
        }

        // Valid after the first endFrame() call; zero-initialized before that.
        [[nodiscard]] const FrameProfile& lastFrame() const noexcept { return m_lastFrame; }

    private:
        friend class ScopedSection;
        void addSectionTime(ProfileSection section, double milliseconds) noexcept;

        FrameProfile m_current;
        FrameProfile m_lastFrame;
        std::chrono::steady_clock::time_point m_frameStart;
    };
}
