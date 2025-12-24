#pragma once

#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace MoldWing {

class Device;

class VulkanEngine_API Buffer {
public:
    Buffer(Device* device, vk::DeviceSize size,
           vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags properties);
    ~Buffer();

    // Delete copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    vk::Buffer getHandle() const { return buffer; }
    vk::DeviceMemory getMemory() const { return memory; }
    vk::DeviceSize getSize() const { return size; }

    // Map memory for CPU access
    void* map();
    void unmap();

    // Copy data to buffer
    void copyData(const void* data, vk::DeviceSize dataSize);

    // Copy data from buffer to host
    void copyToHost(void* data, vk::DeviceSize dataSize);

    // Helper to create and fill buffer in one call
    template<typename T>
    static Buffer* createWithData(Device* device, const std::vector<T>& data,
                                  vk::BufferUsageFlags usage,
                                  vk::MemoryPropertyFlags properties =
                                      vk::MemoryPropertyFlagBits::eHostVisible |
                                      vk::MemoryPropertyFlagBits::eHostCoherent);

private:
    Device* device;
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
};

// Template implementation
template<typename T>
Buffer* Buffer::createWithData(Device* device, const std::vector<T>& data,
                               vk::BufferUsageFlags usage,
                               vk::MemoryPropertyFlags properties) {
    vk::DeviceSize bufferSize = sizeof(T) * data.size();
    auto* buffer = new Buffer(device, bufferSize, usage, properties);
    buffer->copyData(data.data(), bufferSize);
    return buffer;
}

} // namespace MoldWing
