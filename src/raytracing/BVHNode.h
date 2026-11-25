#pragma once

// GPU-compatible struct layouts (std430 / 16-byte aligned on both CPU and GPU).
// CPU-side versions mirror GLSL std430 layout exactly so we can memcpy to Vulkan buffers.

#include <cstdint>
#include <bit>

namespace nowave
{
// ---- BVH node (48 bytes, 16-byte aligned) -----------------------------------
// GLSL equivalent:
//   struct BVHNode {
//     vec4 minLeft;   // xyz = aabbMin,  w = int(leftChild) packed as float bits
//     vec4 maxRight;  // xyz = aabbMax,  w = int(rightChild) packed as float bits
//     int  triIndex;  // -1 = inner node, >= 0 = leaf
//     int  pad[3];
//   };
struct alignas(16) GPUBVHNode
{
    float aabbMinX, aabbMinY, aabbMinZ;
    float leftChildBits;    // std::bit_cast<float>(int32_t leftChild)
    float aabbMaxX, aabbMaxY, aabbMaxZ;
    float rightChildBits;   // std::bit_cast<float>(int32_t rightChild)
    int32_t triIndex;       // -1 = inner, >= 0 = leaf
    int32_t pad[3];

    static float packChild(int32_t child) noexcept
    {
        return std::bit_cast<float>(child);
    }
};
static_assert(sizeof(GPUBVHNode) == 48);

// ---- Triangle (80 bytes, 16-byte aligned) -----------------------------------
// GLSL equivalent:
//   struct GPUTriangle {
//     vec4 v0;
//     vec4 v1;
//     vec4 v2;
//     vec4 n;
//     int materialIndex;
//     int pad[3];
//   };
struct alignas(16) GPUTriangle
{
    float v0x, v0y, v0z, pad0;
    float v1x, v1y, v1z, pad1;
    float v2x, v2y, v2z, pad2;
    float nx,  ny,  nz,  pad3;
    int32_t materialIndex;
    int32_t padding[3];
};
static_assert(sizeof(GPUTriangle) == 80);

struct alignas(16) GPUMaterial
{
    float absorptionLow;
    float absorptionMid;
    float absorptionHigh;
    float scattering;
};
static_assert(sizeof(GPUMaterial) == 16);

// ---- Three-band ray tap (32 bytes) -----------------------------------------
struct alignas(16) RayTap
{
    int32_t sampleIndex;
    int32_t sourceIndex;
    int32_t receiverIndex;
    int32_t pad0;
    float gainLow;
    float gainMid;
    float gainHigh;
    float pad1;
};
static_assert(sizeof(RayTap) == 32);

// Ray segment for visualisation (48 bytes, std430; mirrors GLSL RaySegment):
//   start[3] / end[3] = broadband energy 0..1, energy* = energy per band,
//   reflectionIndex = number of reflections before this segment (0 = direct).
struct alignas(16) RaySegment
{
    float start[4];
    float end[4];
    float energyLow;
    float energyMid;
    float energyHigh;
    int32_t reflectionIndex;
};
static_assert(sizeof(RaySegment) == 48);
} // namespace nowave

