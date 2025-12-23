#pragma once
#include "Export.h"
#include "Instance.h"
#include "Device.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "CommandBufferManager.h"
#include <vulkan/vulkan.hpp>
#include <memory>
#include <functional>

struct GLFWwindow;

namespace VulkanEngine {

struct EngineConfig {
    std::string appName = "VulkanApp";
    uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
    uint32_t width = 800;
    uint32_t height = 600;
    uint32_t maxFramesInFlight = 2;
};

class VulkanEngine_API Engine {
public:
    Engine(GLFWwindow* window, const EngineConfig& config = EngineConfig{});
    ~Engine();

    // Getters
    Instance* getInstance() { return instance.get(); }
    Device* getDevice() { return device.get(); }
    Swapchain* getSwapchain() { return swapchain.get(); }
    RenderPass* getRenderPass() { return renderPass.get(); }
    CommandBufferManager* getCommandBufferManager() { return cmdBufferManager.get(); }
    vk::SurfaceKHR getSurface() const { return surface; }

    // Frame rendering
    using RecordCommandBufferCallback = std::function<void(vk::CommandBuffer, uint32_t imageIndex)>;
    void drawFrame(RecordCommandBufferCallback recordCallback);

    void waitIdle();

private:
    void createSurface(GLFWwindow* window);
    void cleanup();

    std::unique_ptr<Instance> instance;
    std::unique_ptr<Device> device;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<RenderPass> renderPass;
    std::unique_ptr<CommandBufferManager> cmdBufferManager;

    vk::SurfaceKHR surface;
    EngineConfig config;
    size_t currentFrame = 0;
};

} // namespace VulkanEngine
