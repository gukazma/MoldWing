#include <MoldWing/VulkanEngine/Buffer.h>
#include <MoldWing/VulkanEngine/Device.h>
#include <stdexcept>
#include <cstring>

namespace MoldWing {

Buffer::Buffer(Device* device, vk::DeviceSize size,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties)
    : device(device), size(size) {

    // Create buffer
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = device->getHandle().createBuffer(bufferInfo);

    // Allocate memory
    vk::MemoryRequirements memRequirements = device->getHandle().getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    memory = device->getHandle().allocateMemory(allocInfo);

    // Bind buffer to memory
    device->getHandle().bindBufferMemory(buffer, memory, 0);
}

Buffer::~Buffer() {
    if (device) {
        if (buffer) {
            device->getHandle().destroyBuffer(buffer);
        }
        if (memory) {
            device->getHandle().freeMemory(memory);
        }
    }
}

void* Buffer::map() {
    return device->getHandle().mapMemory(memory, 0, size);
}

void Buffer::unmap() {
    device->getHandle().unmapMemory(memory);
}

void Buffer::copyData(const void* data, vk::DeviceSize dataSize) {
    if (dataSize > size) {
        throw std::runtime_error("Data size exceeds buffer size");
    }

    void* mappedData = map();
    std::memcpy(mappedData, data, static_cast<size_t>(dataSize));
    unmap();
}

uint32_t Buffer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties =
        device->getPhysicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace MoldWing
