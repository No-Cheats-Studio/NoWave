#pragma once

#include "raytracing/BVHNode.h"
#include "scene/RoomMesh.h"

#include <vector>

namespace nowave
{
struct BVHBuildResult
{
    std::vector<GPUBVHNode> nodes;
    std::vector<GPUTriangle> triangles;
    std::vector<GPUMaterial> materials;
    bool success { false };
};

class BVHBuilder
{
public:
    BVHBuilder() = delete;

    /**
     * Builds a binary BVH from a RoomMesh using recursive median-split.
     * Each leaf holds exactly one triangle.
     * Returns flat arrays ready for GPU upload.
     */
    static BVHBuildResult build(const RoomMesh& mesh);

private:
    static int buildRecursive(std::vector<GPUBVHNode>& nodes,
                              int& nextNode,
                              const std::vector<GPUTriangle>& gpuTris,
                              const std::vector<glm::vec3>& centroids,
                              std::vector<int>& indices,
                              int start,
                              int end);

    static void computeAABB(const std::vector<GPUTriangle>& gpuTris,
                            const std::vector<int>& indices,
                            int start,
                            int end,
                            glm::vec3& outMin,
                            glm::vec3& outMax);
};
} // namespace nowave

