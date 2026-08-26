#pragma once

#include <string>

namespace spatial::data
{
    // CPU-side material description; GPU binding is added in Phase 8 via MaterialResource.
    struct Material
    {
        std::string name;

        float baseColorR = 1.0f;
        float baseColorG = 1.0f;
        float baseColorB = 1.0f;
        float baseColorA = 1.0f;

        float metallic = 0.0f;
        float roughness = 1.0f;

        [[nodiscard]] bool operator==(const Material&) const = default;
    };
}
