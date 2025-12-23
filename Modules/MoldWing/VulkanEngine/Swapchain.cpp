#include "Swapchain.h"
#include <stdexcept>
#include <algorithm>

namespace VulkanEngine {

Swapchain::Swapchain(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface,
                     uint32_t width, uint32_t height, const QueueFamilyIndices& indices)
    : device(device), physicalDevice(physicalDevice) {
    createSwapchain(physicalDevice, device, surface, width, height, indices);
    createImageViews(device);
    createDepthResources(physicalDevice, device);
}

Swapchain::~Swapchain() {
    if (depthImageView) {
        device.destroyImageView(depthImageView);
    }
    if (depthImage) {
        device.destroyImage(depthImage);
    }
    if (depthImageMemory) {
        device.freeMemory(depthImageMemory);
    }

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

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void Swapchain::createDepthResources(vk::PhysicalDevice physicalDevice, vk::Device device) {
    depthFormat = findDepthFormat(physicalDevice);

    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    depthImage = device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = device.getImageMemoryRequirements(depthImage);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                vk::MemoryPropertyFlagBits::eDeviceLocal);

    depthImageMemory = device.allocateMemory(allocInfo);
    device.bindImageMemory(depthImage, depthImageMemory, 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = depthImage;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    depthImageView = device.createImageView(viewInfo);
}

vk::Format Swapchain::findSupportedFormat(vk::PhysicalDevice physicalDevice,
                                         const std::vector<vk::Format>& candidates,
                                         vk::ImageTiling tiling,
                                         vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

vk::Format Swapchain::findDepthFormat(vk::PhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

} // namespace VulkanEngine
