#include "raytracing/VulkanRaytracer.h"

#include "raytracing/BVHBuilder.h"
#include "raytracer_spv.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace nowave
{
namespace
{
constexpr uint32_t kDescriptorCount = 8;
}

VulkanRaytracer::~VulkanRaytracer()
{
    if (context.getDevice() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context.getDevice());
        destroyBuffer(rayPathCounterBuffer);
        destroyBuffer(rayPathBuffer);
        destroyBuffer(counterBuffer);
        destroyBuffer(tapBuffer);
        destroyBuffer(parametersBuffer);
        destroyBuffer(materialBuffer);
        destroyBuffer(triangleBuffer);
        destroyBuffer(nodeBuffer);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(context.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(context.getDevice(), pipelineLayout, nullptr);
        if (descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(context.getDevice(), descriptorPool, nullptr);
        if (descriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(context.getDevice(), descriptorSetLayout, nullptr);
    }
}

bool VulkanRaytracer::initialize()
{
    if (isAvailable())
        return true;

    if (!context.initialize())
    {
        lastError = context.getLastError();
        return false;
    }

    if (!createPipeline() ||
        !createBuffer(static_cast<VkDeviceSize>(kMaxRayTaps) * sizeof(RayTap),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, tapBuffer) ||
        !createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, counterBuffer) ||
        !createBuffer(static_cast<VkDeviceSize>(kMaxRaySegments) * sizeof(RaySegment),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, rayPathBuffer) ||
        !createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      rayPathCounterBuffer) ||
        !createBuffer(sizeof(RaytraceParameters), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      parametersBuffer))
        return false;

    return true;
}

bool VulkanRaytracer::createPipeline()
{
    VkDescriptorSetLayoutBinding bindings[kDescriptorCount] {};
    for (uint32_t i = 0; i < kDescriptorCount; ++i)
    {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo setLayoutInfo {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    };
    setLayoutInfo.bindingCount = kDescriptorCount;
    setLayoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(context.getDevice(), &setLayoutInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan descriptor set layout.";
        return false;
    }

    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    if (vkCreatePipelineLayout(context.getDevice(), &layoutInfo, nullptr,
                               &pipelineLayout) != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan compute pipeline layout.";
        return false;
    }

    VkShaderModuleCreateInfo shaderInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    shaderInfo.codeSize = shaders::kRaytracerSPVSizeBytes;
    shaderInfo.pCode = reinterpret_cast<const uint32_t*>(shaders::kRaytracerSPV);
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(context.getDevice(), &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan raytracing shader module.";
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    };
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayout;
    const VkResult pipelineResult = vkCreateComputePipelines(
        context.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(context.getDevice(), shaderModule, nullptr);
    if (pipelineResult != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan raytracing compute pipeline.";
        return false;
    }

    VkDescriptorPoolSize poolSize {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = kDescriptorCount;
    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr,
                               &descriptorPool) != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan descriptor pool.";
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    if (vkAllocateDescriptorSets(context.getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        lastError = "Failed to allocate Vulkan descriptor set.";
        return false;
    }
    return true;
}

bool VulkanRaytracer::createBuffer(VkDeviceSize size,
                                   VkBufferUsageFlags usage,
                                   Buffer& buffer)
{
    VkBufferCreateInfo bufferInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(context.getDevice(), &bufferInfo, nullptr, &buffer.handle) != VK_SUCCESS)
    {
        lastError = "Failed to create Vulkan storage buffer.";
        return false;
    }

    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(context.getDevice(), buffer.handle, &requirements);
    const uint32_t memoryType = context.findMemoryType(
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memoryType == UINT32_MAX)
    {
        lastError = "No host-visible coherent Vulkan memory type is available.";
        destroyBuffer(buffer);
        return false;
    }

    VkMemoryAllocateInfo allocInfo { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;
    if (vkAllocateMemory(context.getDevice(), &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS)
    {
        lastError = "Failed to allocate Vulkan buffer memory.";
        destroyBuffer(buffer);
        return false;
    }
    if (vkBindBufferMemory(context.getDevice(), buffer.handle, buffer.memory, 0) != VK_SUCCESS)
    {
        lastError = "Failed to bind Vulkan buffer memory.";
        destroyBuffer(buffer);
        return false;
    }

    buffer.size = size;
    return true;
}

bool VulkanRaytracer::writeBuffer(const Buffer& buffer,
                                 const void* data,
                                 VkDeviceSize size)
{
    if (data == nullptr || size > buffer.size)
    {
        lastError = "Invalid Vulkan buffer upload size.";
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(context.getDevice(), buffer.memory, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        lastError = "Failed to map Vulkan buffer memory for upload.";
        return false;
    }
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(context.getDevice(), buffer.memory);
    return true;
}

void VulkanRaytracer::destroyBuffer(Buffer& buffer) noexcept
{
    if (context.getDevice() != VK_NULL_HANDLE)
    {
        if (buffer.handle != VK_NULL_HANDLE)
            vkDestroyBuffer(context.getDevice(), buffer.handle, nullptr);
        if (buffer.memory != VK_NULL_HANDLE)
            vkFreeMemory(context.getDevice(), buffer.memory, nullptr);
    }
    buffer = {};
}

void VulkanRaytracer::updateDescriptors()
{
    const Buffer* buffers[] = {
        &nodeBuffer, &triangleBuffer, &materialBuffer, &tapBuffer, &counterBuffer,
        &parametersBuffer, &rayPathBuffer, &rayPathCounterBuffer
    };
    VkDescriptorBufferInfo bufferInfos[kDescriptorCount] {};
    VkWriteDescriptorSet writes[kDescriptorCount] {};
    for (uint32_t i = 0; i < kDescriptorCount; ++i)
    {
        bufferInfos[i].buffer = buffers[i]->handle;
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = buffers[i]->size;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufferInfos[i];
    }
    vkUpdateDescriptorSets(context.getDevice(), kDescriptorCount, writes, 0, nullptr);
}

bool VulkanRaytracer::setScene(const RoomMesh& mesh)
{
    auto build = BVHBuilder::build(mesh);
    if (!build.success)
    {
        lastError = "Could not build a BVH for the loaded room mesh.";
        return false;
    }

    VkPhysicalDeviceProperties properties {};
    vkGetPhysicalDeviceProperties(context.getPhysDevice(), &properties);
    const VkDeviceSize maxRange = properties.limits.maxStorageBufferRange;
    const VkDeviceSize nodeBytes = static_cast<VkDeviceSize>(build.nodes.size()) * sizeof(GPUBVHNode);
    const VkDeviceSize triangleBytes =
        static_cast<VkDeviceSize>(build.triangles.size()) * sizeof(GPUTriangle);
    const VkDeviceSize materialBytes =
        static_cast<VkDeviceSize>(build.materials.size()) * sizeof(GPUMaterial);
    if (nodeBytes > maxRange || triangleBytes > maxRange || materialBytes > maxRange)
    {
        lastError = "Room mesh exceeds the Vulkan storage-buffer size limit.";
        return false;
    }

    Buffer newNodes, newTriangles, newMaterials;
    if (!createBuffer(nodeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, newNodes) ||
        !createBuffer(triangleBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, newTriangles) ||
        !createBuffer(materialBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, newMaterials))
    {
        destroyBuffer(newMaterials);
        destroyBuffer(newTriangles);
        destroyBuffer(newNodes);
        return false;
    }

    if (!writeBuffer(newNodes, build.nodes.data(), nodeBytes) ||
        !writeBuffer(newTriangles, build.triangles.data(), triangleBytes) ||
        !writeBuffer(newMaterials, build.materials.data(), materialBytes))
    {
        destroyBuffer(newMaterials);
        destroyBuffer(newTriangles);
        destroyBuffer(newNodes);
        return false;
    }

    vkDeviceWaitIdle(context.getDevice());
    destroyBuffer(nodeBuffer);
    destroyBuffer(triangleBuffer);
    destroyBuffer(materialBuffer);
    nodeBuffer = newNodes;
    triangleBuffer = newTriangles;
    materialBuffer = newMaterials;
    nodes = std::move(build.nodes);
    triangles = std::move(build.triangles);
    materials = std::move(build.materials);
    updateDescriptors();
    return true;
}

bool VulkanRaytracer::trace(const RaytraceParameters& parameters,
                            int numRays, int maxBounces, IRBuffer& output,
                            std::vector<RaySegment>& rayPaths)
{
    rayPaths.clear();
    if (!isAvailable() || nodeBuffer.handle == VK_NULL_HANDLE ||
        numRays <= 0 || maxBounces < 0 ||
        parameters.configuration[0] < 1 || parameters.configuration[0] > 2 ||
        parameters.configuration[1] < 1 || parameters.configuration[1] > 2 ||
        parameters.physics[2] <= 0.0f)
    {
        lastError = "Raytracer is not initialized with a valid scene or parameters.";
        return false;
    }

    output.clear();
    const uint32_t zero = 0;
    RaytraceParameters uploadedParameters = parameters;
    uploadedParameters.limits[0] = std::min(
        uploadedParameters.limits[0], static_cast<int>(output.channels[0].size()));
    uploadedParameters.limits[1] = kMaxRayTaps;
    uploadedParameters.limits[2] = numRays;
    uploadedParameters.limits[3] = maxBounces;
    if (uploadedParameters.limits[0] <= 0 ||
        !writeBuffer(counterBuffer, &zero, sizeof(zero)) ||
        !writeBuffer(rayPathCounterBuffer, &zero, sizeof(zero)) ||
        !writeBuffer(parametersBuffer, &uploadedParameters, sizeof(uploadedParameters)))
        return false;
    output.sourceCount = uploadedParameters.configuration[0];
    output.receiverCount = uploadedParameters.configuration[1];

    VkCommandBufferAllocateInfo allocInfo {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    };
    allocInfo.commandPool = context.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(context.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        lastError = "Failed to allocate Vulkan command buffer.";
        return false;
    }

    VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bool success = vkBeginCommandBuffer(commandBuffer, &beginInfo) == VK_SUCCESS;
    if (success)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        const int totalRays = numRays * uploadedParameters.configuration[0];
        vkCmdDispatch(commandBuffer, static_cast<uint32_t>((totalRays + 63) / 64), 1, 1);
        success = vkEndCommandBuffer(commandBuffer) == VK_SUCCESS;
    }

    if (success)
    {
        VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        success = vkQueueSubmit(context.getComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS &&
                  vkQueueWaitIdle(context.getComputeQueue()) == VK_SUCCESS;
    }
    vkFreeCommandBuffers(context.getDevice(), context.getCommandPool(), 1, &commandBuffer);
    if (!success)
    {
        lastError = "Vulkan raytracing command submission failed.";
        return false;
    }

    uint32_t* tapCount = nullptr;
    if (vkMapMemory(context.getDevice(), counterBuffer.memory, 0, sizeof(uint32_t), 0,
                    reinterpret_cast<void**>(&tapCount)) != VK_SUCCESS)
    {
        lastError = "Failed to map Vulkan tap counter.";
        return false;
    }
    const uint32_t count = std::min(*tapCount, static_cast<uint32_t>(kMaxRayTaps));
    vkUnmapMemory(context.getDevice(), counterBuffer.memory);

    RayTap* taps = nullptr;
    const VkDeviceSize tapBytes = static_cast<VkDeviceSize>(count) * sizeof(RayTap);
    if (tapBytes > 0 &&
        vkMapMemory(context.getDevice(), tapBuffer.memory, 0, tapBytes, 0,
                    reinterpret_cast<void**>(&taps)) != VK_SUCCESS)
    {
        lastError = "Failed to map Vulkan ray tap buffer.";
        return false;
    }

    int sampleLength = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const RayTap& tap = taps[i];
        if (tap.sampleIndex < 0 || tap.sampleIndex >= uploadedParameters.limits[0] ||
            tap.sourceIndex < 0 || tap.sourceIndex >= output.sourceCount ||
            tap.receiverIndex < 0 || tap.receiverIndex >= output.receiverCount)
            continue;
        const size_t receiver = static_cast<size_t>(tap.receiverIndex);
        const size_t source = static_cast<size_t>(tap.sourceIndex);
        const float bandGains[] = { tap.gainLow, tap.gainMid, tap.gainHigh };
        for (size_t band = 0; band < IRBuffer::kBandCount; ++band)
        {
            const size_t route = IRBuffer::channelIndex(band, receiver, source);
            output.channels[route][static_cast<size_t>(tap.sampleIndex)] += bandGains[band];
        }
        sampleLength = std::max(sampleLength, tap.sampleIndex + 1);
    }
    if (tapBytes > 0)
        vkUnmapMemory(context.getDevice(), tapBuffer.memory);

    if (uploadedParameters.physics[3] > 0.5f)
    {
        uint32_t* rayPathCount = nullptr;
        if (vkMapMemory(context.getDevice(), rayPathCounterBuffer.memory, 0,
                        sizeof(uint32_t), 0,
                        reinterpret_cast<void**>(&rayPathCount)) != VK_SUCCESS)
        {
            lastError = "Failed to map Vulkan ray path counter.";
            return false;
        }
        const uint32_t pathCount =
            std::min(*rayPathCount, static_cast<uint32_t>(kMaxRaySegments));
        vkUnmapMemory(context.getDevice(), rayPathCounterBuffer.memory);

        const VkDeviceSize pathBytes =
            static_cast<VkDeviceSize>(pathCount) * sizeof(RaySegment);
        RaySegment* paths = nullptr;
        if (pathBytes > 0 &&
            vkMapMemory(context.getDevice(), rayPathBuffer.memory, 0, pathBytes, 0,
                        reinterpret_cast<void**>(&paths)) != VK_SUCCESS)
        {
            lastError = "Failed to map Vulkan ray path buffer.";
            return false;
        }
        if (pathBytes > 0)
        {
            rayPaths.assign(paths, paths + pathCount);
            vkUnmapMemory(context.getDevice(), rayPathBuffer.memory);
        }
    }

    if (output.receiverCount == 1)
    {
        for (size_t band = 0; band < IRBuffer::kBandCount; ++band)
        {
            const size_t bandOffset = band * IRBuffer::kRoutesPerBand;
            std::copy(output.channels[bandOffset].begin(),
                      output.channels[bandOffset].begin() + sampleLength,
                      output.channels[bandOffset + IRBuffer::kRightFromLeft].begin());
            std::copy(output.channels[bandOffset + IRBuffer::kLeftFromRight].begin(),
                      output.channels[bandOffset + IRBuffer::kLeftFromRight].begin() + sampleLength,
                      output.channels[bandOffset + IRBuffer::kRightFromRight].begin());
        }
    }
    output.sampleLength = sampleLength;
    return true;
}
} // namespace nowave
