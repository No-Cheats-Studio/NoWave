#pragma once

#include <cstdint>
#include <string>

namespace nowave
{
enum class SurfaceTexture : int32_t
{
    Neutral = 0,
    Stone = 1,
    Glass = 2,
    Brick = 3,
    Wood = 4,
    Curtain = 5,
    MineralWool = 6,
    Concrete = 7,
    Carpet = 8,
    Plaster = 9,
    CeilingTile = 10
};

/**
 * Frequency-dependent acoustic properties and procedural viewport material.
 * Stored per material slot and referenced by Triangle::materialId.
 */
struct AcousticMaterial
{
    std::string name { "Default" };
    float absorptionLow { 0.15f };
    float absorptionMid { 0.12f };
    float absorptionHigh { 0.18f };
    float scattering { 0.10f };
    SurfaceTexture texture { SurfaceTexture::Neutral };
};
} // namespace nowave

