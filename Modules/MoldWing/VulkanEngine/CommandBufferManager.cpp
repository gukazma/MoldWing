#include "CommandBufferManager.h"
#include <stdexcept>

namespace MoldWing {

CommandBufferManager::CommandBufferManager(vk::Device device, uint32_t queueFamilyIndex, uint32_t maxFramesInFlight)
    : device(device) {
    createCommandPool(device, queueFamilyIndex);
    createCommandBuffers(device, maxFramesInFlight);
    createSyncObjects(device, maxFramesInFlight);
}

CommandBufferManager::~CommandBufferManager() {
    for (size_t i = 0; i < inFlightFences.size(); i++) {
        device.destroySemaphore(renderFinishedSemaphores[i]);
        device.destroySemaphore(imageAvailableSemaphores[i]);
        device.destroyFence(inFlightFences[i]);
    }

    if (commandPool) {
        device.destroyCommandPool(commandPool);
    }
}

void CommandBufferManager::createCommandPool(vk::Device device, uint32_t queueFamilyIndex) {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    commandPool = device.createCommandPool(poolInfo);
    if (!commandPool) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void CommandBufferManager::createCommandBuffers(vk::Device device, uint32_t count) {
    commandBuffers.resize(count);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = count;

    commandBuffers = device.allocateCommandBuffers(allocInfo);
}

void CommandBufferManager::createSyncObjects(vk::Device device, uint32_t count) {
    imageAvailableSemaphores.resize(count);
    renderFinishedSemaphores.resize(count);
    inFlightFences.resize(count);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i = 0; i < count; i++) {
        imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
        renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
        inFlightFences[i] = device.createFence(fenceInfo);

        if (!imageAvailableSemaphores[i] || !renderFinishedSemaphores[i] || !inFlightFences[i]) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }
}

} // namespace MoldWing
