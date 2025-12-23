#pragma once

#include "Export.h"
#include "Device.h"
#include <vulkan/vulkan.hpp>

namespace MoldWing {

class VulkanEngine_API Image {
  public:
    Image(Device* device, uint32_t width, uint32_t height, vk::Format format,
          vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

    ~Image();

    void transitionLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyFromBuffer(vk::Buffer buffer, uint32_t width, uint32_t height);

    vk::Image getHandle() const { return image; }
    vk::ImageView getImageView() const { return imageView; }
    vk::Format getFormat() const { return format; }

  private:
    Device* device;
    vk::Image image;
    vk::DeviceMemory imageMemory;
    vk::ImageView imageView;
    vk::Format format;

    void createImageView();
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
};

class VulkanEngine_API Sampler {
  public:
    Sampler(Device* device, vk::Filter magFilter = vk::Filter::eLinear,
            vk::Filter minFilter = vk::Filter::eLinear,
            vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat);

    ~Sampler();

    vk::Sampler getHandle() const { return sampler; }

  private:
    Device* device;
    vk::Sampler sampler;
};

} // namespace MoldWing
