#include "Device.h"
#include <stdexcept>
#include <set>
#include <algorithm>

namespace MoldWing {

Device::Device(vk::Instance instance, vk::SurfaceKHR surface)
    : rayTracingEnabled(false), rayTracingSupported(false) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
}

Device::Device(vk::Instance instance, vk::SurfaceKHR surface, bool enableRayTracing)
    : rayTracingEnabled(enableRayTracing), rayTracingSupported(false) {
    pickPhysicalDevice(instance, surface);
    createLogicalDevice();
}

Device::~Device() {
    if (device) {
        device.destroy();
    }
}

vk::PhysicalDeviceRayTracingPipelinePropertiesKHR Device::getRayTracingProperties() const {
    if (!rayTracingSupported) {
        throw std::runtime_error("Ray tracing is not supported on this device");
    }
    return rayTracingProperties;
}

bool Device::checkRayTracingSupport(vk::PhysicalDevice device) {
    // Get available device extensions
    auto availableExtensions = device.enumerateDeviceExtensionProperties();

    // Required ray tracing extensions
    std::vector<const char*> requiredExtensions = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    // Check if all required extensions are available
    for (const char* requiredExt : requiredExtensions) {
        bool found = false;
        for (const auto& availableExt : availableExtensions) {
            if (strcmp(requiredExt, availableExt.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    // Check for ray tracing features using proper structure chain
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.pNext = nullptr;

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
    accelStructFeatures.pNext = &bufferDeviceAddressFeatures;

    vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.pNext = &accelStructFeatures;

    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &rayQueryFeatures;

    device.getFeatures2(&features2);

    return rayQueryFeatures.rayQuery &&
           accelStructFeatures.accelerationStructure &&
           bufferDeviceAddressFeatures.bufferDeviceAddress;
}

void Device::pickPhysicalDevice(vk::Instance instance, vk::SurfaceKHR surface) {
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    // Pick the first suitable device
    for (const auto& dev : devices) {
        auto indices = findQueueFamilies(dev, surface);
        if (!indices.isComplete()) {
            continue;
        }

        // If ray tracing is requested, check support
        if (rayTracingEnabled) {
            if (!checkRayTracingSupport(dev)) {
                continue;
            }
            rayTracingSupported = true;

            // Query ray tracing properties
            rayTracingProperties.sType = vk::StructureType::ePhysicalDeviceRayTracingPipelinePropertiesKHR;
            vk::PhysicalDeviceProperties2 props2;
            props2.pNext = &rayTracingProperties;
            dev.getProperties2(&props2);
        }

        physicalDevice = dev;
        queueFamilyIndices = indices;
        return;
    }

    if (rayTracingEnabled) {
        throw std::runtime_error("Failed to find a suitable GPU with ray tracing support");
    } else {
        throw std::runtime_error("Failed to find a suitable GPU");
    }
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

    // Base device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    // Add ray tracing extensions if enabled and supported
    if (rayTracingEnabled && rayTracingSupported) {
        deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};
    
    // Setup feature chain for ray tracing
    void* pNext = nullptr;
    vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures;
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures;
    
    if (rayTracingEnabled && rayTracingSupported) {
        // Enable ray query features
        rayQueryFeatures.rayQuery = VK_TRUE;
        
        // Enable acceleration structure features
        accelStructFeatures.accelerationStructure = VK_TRUE;
        
        // Enable buffer device address features
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        
        // Chain the structures
        rayQueryFeatures.pNext = &accelStructFeatures;
        accelStructFeatures.pNext = &bufferDeviceAddressFeatures;
        pNext = &rayQueryFeatures;
    }

    vk::DeviceCreateInfo createInfo{};
    createInfo.pNext = pNext;
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

} // namespace MoldWing
