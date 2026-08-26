#pragma once

#include <string_view>

namespace spatial::streaming
{
    // Per-tile resource lifecycle. UploadPending exists for the GPU upload
    // step Phase 8 (Rendering) adds; until then StreamingManager passes a
    // tile through it immediately since there is no GPU resource yet.
    enum class ResourceState
    {
        Unloaded,
        Requested,
        Loading,
        LoadedCPU,
        UploadPending,
        Resident,
        UnloadRequested,
        Unloading,
    };

    [[nodiscard]] constexpr std::string_view toString(ResourceState state) noexcept
    {
        switch (state)
        {
            case ResourceState::Unloaded: return "Unloaded";
            case ResourceState::Requested: return "Requested";
            case ResourceState::Loading: return "Loading";
            case ResourceState::LoadedCPU: return "LoadedCPU";
            case ResourceState::UploadPending: return "UploadPending";
            case ResourceState::Resident: return "Resident";
            case ResourceState::UnloadRequested: return "UnloadRequested";
            case ResourceState::Unloading: return "Unloading";
        }
        return "Unknown";
    }

    // True if `from` -> `to` is a legal single step in the lifecycle above.
    [[nodiscard]] constexpr bool isValidTransition(ResourceState from, ResourceState to) noexcept
    {
        switch (from)
        {
            case ResourceState::Unloaded:
                return to == ResourceState::Requested;
            case ResourceState::Requested:
                return to == ResourceState::Loading || to == ResourceState::Unloaded;
            case ResourceState::Loading:
                return to == ResourceState::LoadedCPU || to == ResourceState::Unloaded;
            case ResourceState::LoadedCPU:
                return to == ResourceState::UploadPending;
            case ResourceState::UploadPending:
                return to == ResourceState::Resident || to == ResourceState::Unloaded;
            case ResourceState::Resident:
                return to == ResourceState::UnloadRequested;
            case ResourceState::UnloadRequested:
                return to == ResourceState::Unloading;
            case ResourceState::Unloading:
                return to == ResourceState::Unloaded;
        }
        return false;
    }
}
