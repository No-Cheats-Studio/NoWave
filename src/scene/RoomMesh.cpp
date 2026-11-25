#include "scene/RoomMesh.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <limits>

namespace nowave
{
float BoundingBox::maxExtent() const noexcept
{
    const glm::vec3 s = size();
    return std::max({ s.x, s.y, s.z });
}

void RoomMesh::recomputeBounds() noexcept
{
    if (vertices.empty())
    {
        bounds = {};
        return;
    }

    constexpr float kMax = std::numeric_limits<float>::max();
    constexpr float kLowest = std::numeric_limits<float>::lowest();

    glm::vec3 minPt(kMax);
    glm::vec3 maxPt(kLowest);

    for (const auto& v : vertices)
    {
        minPt.x = std::min(minPt.x, v.position.x);
        minPt.y = std::min(minPt.y, v.position.y);
        minPt.z = std::min(minPt.z, v.position.z);

        maxPt.x = std::max(maxPt.x, v.position.x);
        maxPt.y = std::max(maxPt.y, v.position.y);
        maxPt.z = std::max(maxPt.z, v.position.z);
    }

    bounds.min = minPt;
    bounds.max = maxPt;
}

void RoomMesh::recomputeNormals() noexcept
{
    for (auto& v : vertices)
        v.normal = glm::vec3(0.0f);

    for (const auto& tri : triangles)
    {
        if (tri.i0 >= vertices.size() || tri.i1 >= vertices.size() || tri.i2 >= vertices.size())
            continue;

        const glm::vec3& p0 = vertices[tri.i0].position;
        const glm::vec3& p1 = vertices[tri.i1].position;
        const glm::vec3& p2 = vertices[tri.i2].position;

        const glm::vec3 crossProd = glm::cross(p1 - p0, p2 - p0);
        const float len = glm::length(crossProd);
        if (len > 1e-8f)
        {
            const glm::vec3 n = crossProd / len;
            vertices[tri.i0].normal += n;
            vertices[tri.i1].normal += n;
            vertices[tri.i2].normal += n;
        }
    }

    for (auto& v : vertices)
    {
        const float len = glm::length(v.normal);
        v.normal = (len > 1e-8f) ? (v.normal / len) : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}
} // namespace nowave

