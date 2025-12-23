#pragma once
#include "Export.h"
#include "Device.h"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace VulkanEngine {

class VulkanEngine_API Swapchain {
public:
    Swapchain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface,
              uint32_t width, uint32_t height, const QueueFamilyIndices& indices);
    ~Swapchain();

    vk::SwapchainKHR getHandle() const { return swapchain; }
    vk::Format getImageFormat() const { return imageFormat; }
    vk::Extent2D getExtent() const { return extent; }
    const std::vector<vk::Image>& getImages() const { return images; }
    const std::vector<vk::ImageView>& getImageViews() const { return imageViews; }

    operator vk::SwapchainKHR() const { return swapchain; }

private:
    void createSwapchain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface,
                        uint32_t width, uint32_t height, const QueueFamilyIndices& indices);
    void createImageViews(vk::Device device);

    vk::Device device;
    vk::SwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::ImageView> imageViews;
    vk::Format imageFormat;
    vk::Extent2D extent;
};

} // namespace VulkanEngine
