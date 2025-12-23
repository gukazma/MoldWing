#include "Swapchain.h"
#include <stdexcept>
#include <algorithm>

namespace VulkanEngine {

Swapchain::Swapchain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface,
                     uint32_t width, uint32_t height, const QueueFamilyIndices& indices)
    : device(device) {
    createSwapchain(physicalDevice, device, surface, width, height, indices);
    createImageViews(device);
}

Swapchain::~Swapchain() {
    for (auto imageView : imageViews) {
        device.destroyImageView(imageView);
    }
    if (swapchain) {
        device.destroySwapchainKHR(swapchain);
    }
}

void Swapchain::createSwapchain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface,
                               uint32_t width, uint32_t height, const QueueFamilyIndices& indices) {
    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    // Choose surface format
    vk::SurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surfaceFormat = availableFormat;
            break;
        }
    }

    imageFormat = surfaceFormat.format;

    // Choose extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent = vk::Extent2D{width, height};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    uint32_t queueFamilyIndicesArray[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = vk::PresentModeKHR::eFifo;
    createInfo.clipped = VK_TRUE;

    swapchain = device.createSwapchainKHR(createInfo);
    if (!swapchain) {
        throw std::runtime_error("Failed to create swapchain");
    }

    images = device.getSwapchainImagesKHR(swapchain);
}

void Swapchain::createImageViews(vk::Device device) {
    imageViews.resize(images.size());

    for (size_t i = 0; i < images.size(); i++) {
        vk::ImageViewCreateInfo createInfo{};
        createInfo.image = images[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = imageFormat;
        createInfo.components.r = vk::ComponentSwizzle::eIdentity;
        createInfo.components.g = vk::ComponentSwizzle::eIdentity;
        createInfo.components.b = vk::ComponentSwizzle::eIdentity;
        createInfo.components.a = vk::ComponentSwizzle::eIdentity;
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        imageViews[i] = device.createImageView(createInfo);
        if (!imageViews[i]) {
            throw std::runtime_error("Failed to create image views");
        }
    }
}

} // namespace VulkanEngine
