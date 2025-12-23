#pragma once

#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace VulkanEngine {

class Device;

class VulkanEngine_API GraphicsPipeline {
public:
    // Constructor using file paths (runtime loading)
    GraphicsPipeline(Device* device, vk::RenderPass renderPass,
                     const std::string& vertShaderPath,
                     const std::string& fragShaderPath,
                     vk::Extent2D extent);

    // Constructor using embedded shader data (compile-time)
    GraphicsPipeline(Device* device, vk::RenderPass renderPass,
                     const uint8_t* vertShaderData, size_t vertShaderSize,
                     const uint8_t* fragShaderData, size_t fragShaderSize,
                     vk::Extent2D extent);

    ~GraphicsPipeline();

    vk::Pipeline getHandle() const { return pipeline; }
    vk::PipelineLayout getLayout() const { return pipelineLayout; }

private:
    Device* device;
    vk::Pipeline pipeline;
    vk::PipelineLayout pipelineLayout;

    vk::ShaderModule vertShaderModule;
    vk::ShaderModule fragShaderModule;

    void createPipeline(vk::RenderPass renderPass,
                       const std::vector<uint8_t>& vertCode,
                       const std::vector<uint8_t>& fragCode,
                       vk::Extent2D extent);

    std::vector<uint8_t> readFile(const std::string& filename);
    vk::ShaderModule createShaderModule(const std::vector<uint8_t>& code);
};

} // namespace VulkanEngine
