#pragma once
#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <optional>

namespace MoldWing {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanEngine_API Device {
public:
    Device(vk::Instance instance, vk::SurfaceKHR surface);
    Device(vk::Instance instance, vk::SurfaceKHR surface, bool enableRayTracing);
    ~Device();

    vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    vk::Device getHandle() const { return device; }
    vk::Queue getGraphicsQueue() const { return graphicsQueue; }
    vk::Queue getPresentQueue() const { return presentQueue; }
    QueueFamilyIndices getQueueFamilyIndices() const { return queueFamilyIndices; }

    bool isRayTracingSupported() const { return rayTracingSupported; }
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR getRayTracingProperties() const;

    operator vk::Device() const { return device; }

private:
    void pickPhysicalDevice(vk::Instance instance, vk::SurfaceKHR surface);
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
    bool checkRayTracingSupport(vk::PhysicalDevice device);

    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    QueueFamilyIndices queueFamilyIndices;

    bool rayTracingEnabled;
    bool rayTracingSupported;
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
};

} // namespace MoldWing
