#pragma once
#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace VulkanEngine {

class VulkanEngine_API RenderPass {
public:
    RenderPass(vk::Device device, vk::Format swapchainImageFormat);
    ~RenderPass();

    vk::RenderPass getHandle() const { return renderPass; }
    operator vk::RenderPass() const { return renderPass; }

    void createFramebuffers(const std::vector<vk::ImageView>& swapchainImageViews,
                           vk::Extent2D extent);
    const std::vector<vk::Framebuffer>& getFramebuffers() const { return framebuffers; }

private:
    vk::Device device;
    vk::RenderPass renderPass;
    std::vector<vk::Framebuffer> framebuffers;
};

} // namespace VulkanEngine
