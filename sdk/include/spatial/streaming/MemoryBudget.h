#pragma once

#include <cstddef>

namespace spatial::streaming
{
    struct MemoryBudgetConfig
    {
        std::size_t cpuBudgetBytes = 512ull * 1024 * 1024;
        std::size_t gpuBudgetBytes = 512ull * 1024 * 1024; // abstraction: no real GPU resources until Phase 8
        std::size_t maxResidentTiles = 256;

        // "Priority points" an untouched cache entry loses per frame when
        // ranking eviction candidates. Higher = more LRU-like; lower = more
        // purely priority/distance-driven. See docs/streaming.md.
        float recencyWeight = 0.01f;
    };
}
