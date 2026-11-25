#include "raytracing/BVHBuilder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include <glm/geometric.hpp>

namespace nowave
{
BVHBuildResult BVHBuilder::build(const RoomMesh& mesh)
{
    BVHBuildResult result;
    if (!mesh.isValid())
        return result;

    result.materials.reserve(std::max<size_t>(mesh.materials.size(), 1));
    for (const auto& material : mesh.materials)
    {
        result.materials.push_back({
            std::clamp(material.absorptionLow, 0.0f, 1.0f),
            std::clamp(material.absorptionMid, 0.0f, 1.0f),
            std::clamp(material.absorptionHigh, 0.0f, 1.0f),
            std::clamp(material.scattering, 0.0f, 1.0f)
        });
    }
    if (result.materials.empty())
        result.materials.push_back({ 0.15f, 0.12f, 0.18f, 0.1f });

    result.triangles.reserve(mesh.triangles.size());
    std::vector<glm::vec3> centroids;
    centroids.reserve(mesh.triangles.size());

    for (const auto& triangle : mesh.triangles)
    {
        if (triangle.i0 >= mesh.vertices.size() ||
            triangle.i1 >= mesh.vertices.size() ||
            triangle.i2 >= mesh.vertices.size())
            return {};

        const auto& p0 = mesh.vertices[triangle.i0].position;
        const auto& p1 = mesh.vertices[triangle.i1].position;
        const auto& p2 = mesh.vertices[triangle.i2].position;
        if (!std::isfinite(p0.x) || !std::isfinite(p0.y) || !std::isfinite(p0.z) ||
            !std::isfinite(p1.x) || !std::isfinite(p1.y) || !std::isfinite(p1.z) ||
            !std::isfinite(p2.x) || !std::isfinite(p2.y) || !std::isfinite(p2.z))
            return {};

        const glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
        const float normalLength = glm::length(normal);
        if (normalLength <= 1.0e-8f)
            continue;

        const uint32_t materialIndex = triangle.materialId < result.materials.size()
                                           ? triangle.materialId
                                           : 0u;
        result.triangles.push_back({
            p0.x, p0.y, p0.z, 0.0f,
            p1.x, p1.y, p1.z, 0.0f,
            p2.x, p2.y, p2.z, 0.0f,
            normal.x / normalLength, normal.y / normalLength, normal.z / normalLength, 0.0f,
            static_cast<int32_t>(materialIndex),
            { 0, 0, 0 }
        });
        centroids.push_back((p0 + p1 + p2) / 3.0f);
    }

    if (result.triangles.empty())
        return {};

    std::vector<int> indices(result.triangles.size());
    std::iota(indices.begin(), indices.end(), 0);
    result.nodes.resize(result.triangles.size() * 2 - 1);
    int nextNode = 0;
    buildRecursive(result.nodes, nextNode, result.triangles, centroids,
                   indices, 0, static_cast<int>(indices.size()));
    result.nodes.resize(static_cast<size_t>(nextNode));
    result.success = !result.nodes.empty();
    return result;
}

int BVHBuilder::buildRecursive(std::vector<GPUBVHNode>& nodes,
                               int& nextNode,
                               const std::vector<GPUTriangle>& gpuTris,
                               const std::vector<glm::vec3>& centroids,
                               std::vector<int>& indices,
                               int start,
                               int end)
{
    const int nodeIndex = nextNode++;
    auto& node = nodes[static_cast<size_t>(nodeIndex)];

    glm::vec3 boundsMin, boundsMax;
    computeAABB(gpuTris, indices, start, end, boundsMin, boundsMax);
    node.aabbMinX = boundsMin.x;
    node.aabbMinY = boundsMin.y;
    node.aabbMinZ = boundsMin.z;
    node.aabbMaxX = boundsMax.x;
    node.aabbMaxY = boundsMax.y;
    node.aabbMaxZ = boundsMax.z;
    node.triIndex = -1;
    node.pad[0] = node.pad[1] = node.pad[2] = 0;

    if (end - start == 1)
    {
        node.leftChildBits = GPUBVHNode::packChild(-1);
        node.rightChildBits = GPUBVHNode::packChild(-1);
        node.triIndex = indices[static_cast<size_t>(start)];
        return nodeIndex;
    }

    glm::vec3 centroidMin = centroids[static_cast<size_t>(indices[static_cast<size_t>(start)])];
    glm::vec3 centroidMax = centroidMin;
    for (int i = start + 1; i < end; ++i)
    {
        const auto& centroid = centroids[static_cast<size_t>(indices[static_cast<size_t>(i)])];
        centroidMin = glm::min(centroidMin, centroid);
        centroidMax = glm::max(centroidMax, centroid);
    }

    const glm::vec3 extent = centroidMax - centroidMin;
    int axis = 0;
    if (extent.y > extent.x && extent.y >= extent.z)
        axis = 1;
    else if (extent.z > extent.x && extent.z > extent.y)
        axis = 2;

    const int middle = start + (end - start) / 2;
    std::nth_element(indices.begin() + start, indices.begin() + middle, indices.begin() + end,
                     [&](int lhs, int rhs)
                     {
                         return centroids[static_cast<size_t>(lhs)][axis] <
                                centroids[static_cast<size_t>(rhs)][axis];
                     });

    const int leftChild = buildRecursive(nodes, nextNode, gpuTris, centroids,
                                         indices, start, middle);
    const int rightChild = buildRecursive(nodes, nextNode, gpuTris, centroids,
                                          indices, middle, end);
    node.leftChildBits = GPUBVHNode::packChild(leftChild);
    node.rightChildBits = GPUBVHNode::packChild(rightChild);
    return nodeIndex;
}

void BVHBuilder::computeAABB(const std::vector<GPUTriangle>& gpuTris,
                             const std::vector<int>& indices,
                             int start,
                             int end,
                             glm::vec3& outMin,
                             glm::vec3& outMax)
{
    const float maxFloat = std::numeric_limits<float>::max();
    outMin = glm::vec3(maxFloat);
    outMax = glm::vec3(-maxFloat);

    for (int i = start; i < end; ++i)
    {
        const auto& triangle = gpuTris[static_cast<size_t>(indices[static_cast<size_t>(i)])];
        const glm::vec3 vertices[] = {
            { triangle.v0x, triangle.v0y, triangle.v0z },
            { triangle.v1x, triangle.v1y, triangle.v1z },
            { triangle.v2x, triangle.v2y, triangle.v2z }
        };
        for (const auto& vertex : vertices)
        {
            outMin = glm::min(outMin, vertex);
            outMax = glm::max(outMax, vertex);
        }
    }
}
} // namespace nowave
