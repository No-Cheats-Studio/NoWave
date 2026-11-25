#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace nowave
{
/**
 * Owns the headless Vulkan instance, physical device, logical device and
 * a single compute queue. No swapchain, no surface – purely for GPU compute.
 *
 * Lifecycle:
 *   VulkanContext ctx;
 *   if (!ctx.initialize())   // log error
 *   ctx.getDevice() ...      // use
 *   // destructor cleans up
 */
class VulkanContext
{
public:
    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /** Names of all Vulkan devices with a compute queue (uses a temporary instance). */
    static std::vector<std::string> listComputeDevices();

    /** preferredDevice: exact device name; empty or unknown name = automatic choice. */
    bool initialize(const std::string& preferredDevice = {}) noexcept;
    bool isValid() const noexcept { return device != VK_NULL_HANDLE; }

    VkInstance       getInstance()    const noexcept { return instance; }
    VkPhysicalDevice getPhysDevice()  const noexcept { return physDevice; }
    VkDevice         getDevice()      const noexcept { return device; }
    VkQueue          getComputeQueue()const noexcept { return computeQueue; }
    VkCommandPool    getCommandPool() const noexcept { return commandPool; }
    uint32_t         getComputeQueueFamilyIndex() const noexcept { return computeQFI; }

    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const noexcept;

    const std::string& getLastError() const noexcept { return lastError; }
    const std::string& getDeviceName() const noexcept { return deviceName; }

private:
    bool createInstance()     noexcept;
    bool selectPhysDevice(const std::string& preferredDevice) noexcept;
    bool createLogicalDevice() noexcept;
    bool createCommandPool()  noexcept;

    VkInstance       instance     { VK_NULL_HANDLE };
    VkPhysicalDevice physDevice   { VK_NULL_HANDLE };
    VkDevice         device       { VK_NULL_HANDLE };
    VkQueue          computeQueue { VK_NULL_HANDLE };
    VkCommandPool    commandPool  { VK_NULL_HANDLE };
    uint32_t         computeQFI   { UINT32_MAX };

    std::string lastError;
    std::string deviceName;
};
} // namespace nowave

