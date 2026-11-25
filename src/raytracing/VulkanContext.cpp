#include "raytracing/VulkanContext.h"

#include <algorithm>
#include <vector>

namespace nowave
{
VulkanContext::~VulkanContext()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
        if (commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, nullptr);
}

bool VulkanContext::initialize() noexcept
{
    if (isValid())
        return true;

    return createInstance() &&
           selectPhysDevice() &&
           createLogicalDevice() &&
           createCommandPool();
}

bool VulkanContext::createInstance() noexcept
{
    VkApplicationInfo applicationInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.pApplicationName = "NoWave";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "NoWave";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &applicationInfo;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        lastError = "vkCreateInstance failed.";
        return false;
    }
    return true;
}

bool VulkanContext::selectPhysDevice() noexcept
{
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS ||
        deviceCount == 0)
    {
        lastError = "No Vulkan physical device is available.";
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS)
    {
        lastError = "Failed to enumerate Vulkan physical devices.";
        return false;
    }

    int bestScore = -1;
    for (const auto candidate : devices)
    {
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        for (uint32_t i = 0; i < familyCount; ++i)
        {
            if (families[i].queueCount == 0 ||
                (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
                continue;

            VkPhysicalDeviceProperties properties {};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            int score = 1;
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score += 2;
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                score += 1;

            if (score > bestScore)
            {
                bestScore = score;
                physDevice = candidate;
                computeQFI = i;
            }
        }
    }

    if (physDevice == VK_NULL_HANDLE)
    {
        lastError = "No Vulkan compute queue is available.";
        return false;
    }
    return true;
}

bool VulkanContext::createLogicalDevice() noexcept
{
    constexpr float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = computeQFI;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures features {};
    VkDeviceCreateInfo createInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.pEnabledFeatures = &features;

    if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        lastError = "vkCreateDevice failed.";
        return false;
    }

    vkGetDeviceQueue(device, computeQFI, 0, &computeQueue);
    return true;
}

bool VulkanContext::createCommandPool() noexcept
{
    VkCommandPoolCreateInfo createInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                       VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = computeQFI;

    if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        lastError = "vkCreateCommandPool failed.";
        return false;
    }
    return true;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeBits,
                                      VkMemoryPropertyFlags props) const noexcept
{
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) != 0 &&
            (memoryProperties.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}
} // namespace nowave
