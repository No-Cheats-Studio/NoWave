#pragma once

#include "scene/AcousticMaterial.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace nowave
{
struct Vertex
{
    glm::vec3 position { 0.0f, 0.0f, 0.0f };
    glm::vec3 normal { 0.0f, 1.0f, 0.0f };
};

struct Triangle
{
    uint32_t i0 { 0 };
    uint32_t i1 { 0 };
    uint32_t i2 { 0 };
    uint32_t materialId { 0 };
};

struct BoundingBox
{
    glm::vec3 min { 0.0f, 0.0f, 0.0f };
    glm::vec3 max { 0.0f, 0.0f, 0.0f };

    glm::vec3 center() const noexcept { return (min + max) * 0.5f; }
    glm::vec3 size() const noexcept { return max - min; }
    float maxExtent() const noexcept;
};

struct RoomMesh
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<AcousticMaterial> materials;
    BoundingBox bounds;

    bool isValid() const noexcept { return !vertices.empty() && !triangles.empty(); }
    void recomputeBounds() noexcept;
    void recomputeNormals() noexcept;
};
} // namespace nowave

