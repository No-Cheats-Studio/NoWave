#include "scene/MaterialLibrary.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace nowave
{
namespace
{
struct KeywordGroup
{
    size_t materialIndex; // index into MaterialLibrary::builtIn()
    std::vector<const char*> words;
};

// Order matters: the first group with a matching keyword wins.
const std::vector<KeywordGroup>& keywordGroups()
{
    static const std::vector<KeywordGroup> groups {
        { 5, { "mineral", "wool", "foam", "absorber", "acoustic_panel", "insulation" } },
        { 9, { "ceiling_tile", "ceilingtile", "tile_ceiling" } },
        { 4, { "curtain", "drape", "velour", "fabric", "cloth", "textile" } },
        { 8, { "carpet", "rug" } },
        { 1, { "glass", "window", "pane", "mirror" } },
        { 3, { "wood", "parquet", "timber", "oak", "plywood", "laminate", "floorboard", "door" } },
        { 2, { "brick" } },
        { 6, { "concrete", "cement", "asphalt", "screed" } },
        { 0, { "stone", "marble", "granite", "tile", "ceramic", "slate" } },
        { 7, { "plaster", "drywall", "gypsum", "paint", "wall", "stucco" } }
    };
    return groups;
}

std::string lowercase(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
} // namespace

std::vector<AcousticMaterial> MaterialLibrary::builtIn()
{
    return {
        { "Polished Stone", 0.01f, 0.01f, 0.02f, 0.05f, SurfaceTexture::Stone },
        { "Glass", 0.15f, 0.06f, 0.03f, 0.03f, SurfaceTexture::Glass },
        { "Brick", 0.03f, 0.04f, 0.07f, 0.30f, SurfaceTexture::Brick },
        { "Wood", 0.12f, 0.10f, 0.09f, 0.15f, SurfaceTexture::Wood },
        { "Velour Curtain", 0.14f, 0.55f, 0.70f, 0.55f, SurfaceTexture::Curtain },
        { "Mineral Wool", 0.45f, 0.95f, 0.97f, 0.25f, SurfaceTexture::MineralWool },
        { "Concrete", 0.01f, 0.02f, 0.02f, 0.20f, SurfaceTexture::Concrete },
        { "Plaster", 0.14f, 0.06f, 0.05f, 0.08f, SurfaceTexture::Plaster },
        { "Carpet", 0.05f, 0.25f, 0.60f, 0.35f, SurfaceTexture::Carpet },
        { "Acoustic Ceiling Tile", 0.30f, 0.70f, 0.75f, 0.20f, SurfaceTexture::CeilingTile }
    };
}

bool MaterialLibrary::matchByName(std::string_view name, AcousticMaterial& material)
{
    const std::string lower = lowercase(name);
    const auto library = builtIn();

    for (const auto& group : keywordGroups())
    {
        for (const char* word : group.words)
        {
            if (lower.find(word) == std::string::npos)
                continue;

            const AcousticMaterial& match = library[group.materialIndex];
            material.absorptionLow = match.absorptionLow;
            material.absorptionMid = match.absorptionMid;
            material.absorptionHigh = match.absorptionHigh;
            material.scattering = match.scattering;
            material.texture = match.texture;
            return true;
        }
    }
    return false;
}
} // namespace nowave
