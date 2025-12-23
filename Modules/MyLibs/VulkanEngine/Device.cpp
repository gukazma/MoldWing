#include "Device.h"
#include <stdexcept>
#include <set>

namespace VulkanEngine {

Device::Device(vk::Instance instance, vk::SurfaceKHR surface) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
}

Device::~Device() {
    if (device) {
        device.destroy();
    }
}

void Device::pickPhysicalDevice(vk::Instance instance, vk::SurfaceKHR surface) {
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    // Pick the first suitable device
    for (const auto& dev : devices) {
        auto indices = findQueueFamilies(dev, surface);
        if (indices.isComplete()) {
            physicalDevice = dev;
            queueFamilyIndices = indices;
            return;
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU");
}

void Device::createLogicalDevice() {
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    device = physicalDevice.createDevice(createInfo);
    if (!device) {
        throw std::runtime_error("Failed to create logical device");
    }

    graphicsQueue = device.getQueue(queueFamilyIndices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(queueFamilyIndices.presentFamily.value(), 0);
}

QueueFamilyIndices Device::findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    QueueFamilyIndices indices;

    auto queueFamilies = device.getQueueFamilyProperties();

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, surface)) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

} // namespace VulkanEngine
