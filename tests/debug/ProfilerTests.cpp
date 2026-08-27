#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "spatial/debug/Profiler.h"

using namespace spatial::debug;

TEST_CASE("Profiler::lastFrame is zero-initialized before any frame completes", "[debug][profiler]")
{
    Profiler profiler;
    const FrameProfile& frame = profiler.lastFrame();
    CHECK(frame.totalMs == 0.0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(ProfileSection::Count); ++i)
    {
        CHECK(frame.sectionMs[i] == 0.0);
    }
}

TEST_CASE("Profiler records a measured section and leaves others at zero", "[debug][profiler]")
{
    Profiler profiler;
    profiler.beginFrame();
    {
        const auto section = profiler.measure(ProfileSection::StreamingUpdate);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    profiler.endFrame();

    const FrameProfile& frame = profiler.lastFrame();
    CHECK(frame.section(ProfileSection::StreamingUpdate) > 0.0);
    CHECK(frame.section(ProfileSection::GPUUpload) == 0.0);
    CHECK(frame.section(ProfileSection::LODSelection) == 0.0);
    CHECK(frame.section(ProfileSection::DebugDraw) == 0.0);
    CHECK(frame.totalMs >= frame.section(ProfileSection::StreamingUpdate));
}

TEST_CASE("Profiler accumulates repeated measure() calls to the same section within one frame", "[debug][profiler]")
{
    Profiler profiler;
    profiler.beginFrame();
    for (int i = 0; i < 3; ++i)
    {
        const auto section = profiler.measure(ProfileSection::LODSelection);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    profiler.endFrame();

    // Three sleeps, each measured independently and summed — the total
    // should be at least roughly 3x a single sleep's contribution, not one.
    Profiler singleShot;
    singleShot.beginFrame();
    {
        const auto section = singleShot.measure(ProfileSection::LODSelection);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    singleShot.endFrame();

    CHECK(profiler.lastFrame().section(ProfileSection::LODSelection) > singleShot.lastFrame().section(ProfileSection::LODSelection));
}

TEST_CASE("Profiler::beginFrame resets section times from the previous frame", "[debug][profiler]")
{
    Profiler profiler;

    profiler.beginFrame();
    {
        const auto section = profiler.measure(ProfileSection::GPUUpload);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    profiler.endFrame();
    REQUIRE(profiler.lastFrame().section(ProfileSection::GPUUpload) > 0.0);

    // Second frame measures nothing — the stale value from frame one must
    // not leak into frame two's result.
    profiler.beginFrame();
    profiler.endFrame();
    CHECK(profiler.lastFrame().section(ProfileSection::GPUUpload) == 0.0);
}

TEST_CASE("Profiler::endFrame sets a non-negative total even with no measured sections", "[debug][profiler]")
{
    Profiler profiler;
    profiler.beginFrame();
    profiler.endFrame();
    CHECK(profiler.lastFrame().totalMs >= 0.0);
}
