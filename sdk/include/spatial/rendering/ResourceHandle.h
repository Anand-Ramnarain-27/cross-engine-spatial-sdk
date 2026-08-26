#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace spatial::rendering
{
    // Opaque GPU resource handle. Phantom-typed on Tag so MeshHandle and
    // TextureHandle can't be mixed up at a call site despite sharing the
    // same representation.
    template <typename Tag>
    struct Handle
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool isValid() const noexcept { return id != 0; }
        [[nodiscard]] constexpr bool operator==(const Handle&) const = default;
    };

    struct MeshTag
    {
    };

    struct TextureTag
    {
    };

    struct MaterialTag
    {
    };

    using MeshHandle = Handle<MeshTag>;
    using TextureHandle = Handle<TextureTag>;
    using MaterialHandle = Handle<MaterialTag>;
}

template <typename Tag>
struct std::hash<spatial::rendering::Handle<Tag>>
{
    [[nodiscard]] std::size_t operator()(const spatial::rendering::Handle<Tag>& handle) const noexcept
    {
        return std::hash<std::uint64_t>{}(handle.id);
    }
};
