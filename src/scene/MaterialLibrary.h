#pragma once

#include "scene/AcousticMaterial.h"

#include <string_view>
#include <vector>

namespace nowave
{
class MaterialLibrary
{
public:
    // Built-in materials; the order matches the material slots used by the room presets.
    static std::vector<AcousticMaterial> builtIn();

    // Maps an imported material name (e.g. "Oak_Floor.001") to a built-in material by
    // keyword. Returns false and leaves `material` untouched if nothing matches.
    static bool matchByName(std::string_view name, AcousticMaterial& material);
};
} // namespace nowave
