#include <MoldWing/VulkanEngine/GraphicsPipeline.h>
#include <MoldWing/VulkanEngine/Device.h>

#include <fstream>
#include <stdexcept>

namespace VulkanEngine {

// Constructor using file paths (runtime loading)
GraphicsPipeline::GraphicsPipeline(Device* device, vk::RenderPass renderPass,
                                   const std::string& vertShaderPath,
                                   const std::string& fragShaderPath,
                                   vk::Extent2D extent)
    : device(device) {

    // Load shader files
    auto vertShaderCode = readFile(vertShaderPath);
    auto fragShaderCode = readFile(fragShaderPath);

    createPipeline(renderPass, vertShaderCode, fragShaderCode, extent);
}

// Constructor using embedded shader data (compile-time)
GraphicsPipeline::GraphicsPipeline(Device* device, vk::RenderPass renderPass,
                                   const uint8_t* vertShaderData, size_t vertShaderSize,
                                   const uint8_t* fragShaderData, size_t fragShaderSize,
                                   vk::Extent2D extent)
    : device(device) {

    // Convert raw data to vectors
    std::vector<uint8_t> vertShaderCode(vertShaderData, vertShaderData + vertShaderSize);
    std::vector<uint8_t> fragShaderCode(fragShaderData, fragShaderData + fragShaderSize);

    createPipeline(renderPass, vertShaderCode, fragShaderCode, extent);
}

GraphicsPipeline::~GraphicsPipeline() {
    if (device) {
        if (pipeline) {
            device->getHandle().destroyPipeline(pipeline);
        }
        if (pipelineLayout) {
            device->getHandle().destroyPipelineLayout(pipelineLayout);
        }
        if (vertShaderModule) {
            device->getHandle().destroyShaderModule(vertShaderModule);
        }
        if (fragShaderModule) {
            device->getHandle().destroyShaderModule(fragShaderModule);
        }
    }
}

void GraphicsPipeline::createPipeline(vk::RenderPass renderPass,
                                     const std::vector<uint8_t>& vertCode,
                                     const std::vector<uint8_t>& fragCode,
                                     vk::Extent2D extent) {
    // Create shader modules
    vertShaderModule = createShaderModule(vertCode);
    fragShaderModule = createShaderModule(fragCode);

    // Shader stage creation
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input (empty for now, vertices are hardcoded in shader)
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = extent;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Color blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    pipelineLayout = device->getHandle().createPipelineLayout(pipelineLayoutInfo);

    // Create graphics pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    auto result = device->getHandle().createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    pipeline = result.value;
}

std::vector<uint8_t> GraphicsPipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint8_t> buffer(fileSize);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

vk::ShaderModule GraphicsPipeline::createShaderModule(const std::vector<uint8_t>& code) {
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return device->getHandle().createShaderModule(createInfo);
}

} // namespace VulkanEngine
