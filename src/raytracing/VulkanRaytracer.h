#pragma once

#include "audio/IRBuffer.h"
#include "raytracing/AcousticSimulation.h"
#include "raytracing/BVHNode.h"
#include "raytracing/VulkanContext.h"
#include "scene/RoomMesh.h"

#include <string>
#include <vector>

namespace nowave
{
class VulkanRaytracer
{
public:
    VulkanRaytracer() = default;
    ~VulkanRaytracer();

    VulkanRaytracer(const VulkanRaytracer&) = delete;
    VulkanRaytracer& operator=(const VulkanRaytracer&) = delete;

    bool initialize();
    bool isAvailable() const noexcept { return context.isValid() && pipeline != VK_NULL_HANDLE; }
    bool setScene(const RoomMesh& mesh);
    bool trace(const RaytraceParameters& parameters,
               int numRays, int maxBounces, IRBuffer& output);

    const std::string& getLastError() const noexcept { return lastError; }

private:
    struct Buffer
    {
        VkBuffer handle { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        VkDeviceSize size { 0 };
    };

    static constexpr int kMaxTaps = 65536;

    bool createPipeline();
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& buffer);
    bool writeBuffer(const Buffer& buffer, const void* data, VkDeviceSize size);
    void destroyBuffer(Buffer& buffer) noexcept;
    void updateDescriptors();

    VulkanContext context;
    VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };
    VkDescriptorPool descriptorPool { VK_NULL_HANDLE };
    VkDescriptorSet descriptorSet { VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout { VK_NULL_HANDLE };
    VkPipeline pipeline { VK_NULL_HANDLE };

    Buffer nodeBuffer;
    Buffer triangleBuffer;
    Buffer materialBuffer;
    Buffer tapBuffer;
    Buffer counterBuffer;
    Buffer parametersBuffer;

    std::vector<GPUBVHNode> nodes;
    std::vector<GPUTriangle> triangles;
    std::vector<GPUMaterial> materials;
    std::string lastError;
};
} // namespace nowave
