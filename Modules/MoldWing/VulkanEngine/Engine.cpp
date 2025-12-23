#include "Engine.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace VulkanEngine {

Engine::Engine(GLFWwindow* window, const EngineConfig& config)
    : config(config) {
    // Get required GLFW extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Create Vulkan instance
    instance = std::make_unique<Instance>(config.appName, config.appVersion, extensions);

    // Create surface
    createSurface(window);

    // Create device
    device = std::make_unique<Device>(instance->getHandle(), surface);

    // Create swapchain
    swapchain = std::make_unique<Swapchain>(
        device->getPhysicalDevice(),
        device->getHandle(),
        surface,
        config.width,
        config.height,
        device->getQueueFamilyIndices()
    );

    // Create render pass
    renderPass = std::make_unique<RenderPass>(
        device->getHandle(),
        swapchain->getImageFormat(),
        swapchain->getDepthFormat()
    );

    // Create framebuffers
    renderPass->createFramebuffers(
        swapchain->getImageViews(),
        swapchain->getDepthImageView(),
        swapchain->getExtent()
    );

    // Create command buffers
    cmdBufferManager = std::make_unique<CommandBufferManager>(
        device->getHandle(),
        device->getQueueFamilyIndices().graphicsFamily.value(),
        config.maxFramesInFlight
    );
}

Engine::~Engine() {
    cleanup();
}

void Engine::createSurface(GLFWwindow* window) {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(instance->getHandle()),
                                window, nullptr, &rawSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface = vk::SurfaceKHR(rawSurface);
}

void Engine::drawFrame(RecordCommandBufferCallback recordCallback) {
    auto device = this->device->getHandle();
    auto fence = cmdBufferManager->getInFlightFence(currentFrame);
    auto imageAvailable = cmdBufferManager->getImageAvailableSemaphore(currentFrame);
    auto renderFinished = cmdBufferManager->getRenderFinishedSemaphore(currentFrame);
    auto commandBuffer = cmdBufferManager->getCommandBuffer(currentFrame);

    // Wait for previous frame
    auto waitResult = device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);

    // Acquire next image
    uint32_t imageIndex;
    auto result = device.acquireNextImageKHR(
        swapchain->getHandle(),
        UINT64_MAX,
        imageAvailable,
        nullptr,
        &imageIndex
    );

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Reset fence
    device.resetFences(1, &fence);

    // Reset and record command buffer
    commandBuffer.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    // Call user callback to record commands
    recordCallback(commandBuffer, imageIndex);

    commandBuffer.end();

    // Submit command buffer
    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = {imageAvailable};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vk::Semaphore signalSemaphores[] = {renderFinished};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    auto submitResult = this->device->getGraphicsQueue().submit(1, &submitInfo, fence);

    // Present
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    vk::SwapchainKHR swapchains[] = {swapchain->getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    auto presentResult = this->device->getPresentQueue().presentKHR(presentInfo);

    currentFrame = (currentFrame + 1) % config.maxFramesInFlight;
}

void Engine::waitIdle() {
    device->getHandle().waitIdle();
}

void Engine::cleanup() {
    if (device) {
        device->getHandle().waitIdle();
    }

    cmdBufferManager.reset();
    renderPass.reset();
    swapchain.reset();
    device.reset();

    if (surface && instance) {
        instance->getHandle().destroySurfaceKHR(surface);
    }

    instance.reset();
}

} // namespace VulkanEngine
