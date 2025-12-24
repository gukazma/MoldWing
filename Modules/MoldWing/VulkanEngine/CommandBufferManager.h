#pragma once
#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace MoldWing {

class VulkanEngine_API CommandBufferManager {
public:
    CommandBufferManager(vk::Device device, uint32_t queueFamilyIndex, uint32_t maxFramesInFlight);
    ~CommandBufferManager();

    vk::CommandPool getCommandPool() const { return commandPool; }
    const std::vector<vk::CommandBuffer>& getCommandBuffers() const { return commandBuffers; }
    vk::CommandBuffer getCommandBuffer(size_t index) const { return commandBuffers[index]; }

    // Synchronization objects
    const std::vector<vk::Semaphore>& getImageAvailableSemaphores() const { return imageAvailableSemaphores; }
    const std::vector<vk::Semaphore>& getRenderFinishedSemaphores() const { return renderFinishedSemaphores; }
    const std::vector<vk::Fence>& getInFlightFences() const { return inFlightFences; }

    vk::Semaphore getImageAvailableSemaphore(size_t index) const { return imageAvailableSemaphores[index]; }
    vk::Semaphore getRenderFinishedSemaphore(size_t index) const { return renderFinishedSemaphores[index]; }
    vk::Fence getInFlightFence(size_t index) const { return inFlightFences[index]; }

    // Single time command helpers
    void setGraphicsQueue(vk::Queue queue) { graphicsQueue = queue; }
    vk::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

private:
    void createCommandPool(vk::Device device, uint32_t queueFamilyIndex);
    void createCommandBuffers(vk::Device device, uint32_t count);
    void createSyncObjects(vk::Device device, uint32_t count);

    vk::Device device;
    vk::Queue graphicsQueue;
    vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> commandBuffers;
    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
};

} // namespace MoldWing
