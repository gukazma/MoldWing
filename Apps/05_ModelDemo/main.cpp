#include <MoldWing/VulkanEngine/Engine.h>
#include <MoldWing/VulkanEngine/GraphicsPipeline.h>
#include <MoldWing/VulkanEngine/Buffer.h>
#include <MoldWing/VulkanEngine/Image.h>
#include <MoldWing/VulkanEngine/TextureLoader.h>
#include <MoldWing/Mesh/Mesh.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Include generated shader headers
#include <MoldWing/Shaders/model.vert.h>
#include <MoldWing/Shaders/model.frag.h>

#include <iostream>
#include <stdexcept>
#include <array>

// Vertex structure matching shader
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        // Position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Normal attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // TexCoord attribute
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

// Uniform Buffer Object matching shader
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class ModelDemo {
  public:
    void run() {
        initWindow();
        initVulkan();
        loadTexture();
        loadModel();
        createBuffers();
        createDescriptorSets();
        createGraphicsPipeline();
        mainLoop();
        cleanup();
    }

  private:
    GLFWwindow* window = nullptr;
    MoldWing::Engine* engine = nullptr;
    MoldWing::GraphicsPipeline* pipeline = nullptr;

    MoldWing::Buffer* vertexBuffer = nullptr;
    MoldWing::Buffer* indexBuffer = nullptr;
    std::vector<MoldWing::Buffer*> uniformBuffers;

    MoldWing::Image* textureImage = nullptr;
    MoldWing::Sampler* textureSampler = nullptr;

    vk::DescriptorSetLayout descriptorSetLayout;
    vk::DescriptorPool descriptorPool;
    std::vector<vk::DescriptorSet> descriptorSets;

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    float rotation = 0.0f;

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "05 Model Demo - OBJ Loader", nullptr, nullptr);
    }

    void initVulkan() {
        MoldWing::EngineConfig config;
        config.appName = "ModelDemo";
        config.width = WIDTH;
        config.height = HEIGHT;
        config.maxFramesInFlight = MAX_FRAMES_IN_FLIGHT;

        engine = new MoldWing::Engine(window, config);
    }

    void loadTexture() {
        std::string texturePath = "assets/textures/cube_texture.png";
        std::cout << "Loading texture: " << texturePath << std::endl;

        textureImage = MoldWing::TextureLoader::loadTexture(engine->getDevice(), texturePath);
        textureSampler = new MoldWing::Sampler(engine->getDevice());

        std::cout << "Texture loaded successfully" << std::endl;
    }

    void loadModel() {
        // Load OBJ model using Mesh class
        MoldWing::Mesh mesh;

        std::string modelPath = "assets/models/cube.obj";
        if (!mesh.loadFromOBJ(modelPath)) {
            throw std::runtime_error("Failed to load model: " + modelPath);
        }

        // Center and normalize the model
        mesh.centerAtOrigin();
        mesh.normalizeScale();

        std::cout << "Model loaded successfully:" << std::endl;
        std::cout << "  Vertices: " << mesh.getVertexCount() << std::endl;
        std::cout << "  Triangles: " << mesh.getTriangleCount() << std::endl;

        // Convert mesh to render data
        std::vector<float> renderData;
        std::vector<uint32_t> indices32;
        mesh.toRenderData(renderData, indices32);

        // Convert to Vertex structs with normals and UVs
        size_t vertexCount = renderData.size() / 8; // pos(3) + normal(3) + texCoord(2)
        vertices.reserve(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
            size_t offset = i * 8;
            Vertex vertex{};
            vertex.pos = glm::vec3(renderData[offset + 0], renderData[offset + 1],
                                   renderData[offset + 2]);
            vertex.normal = glm::vec3(renderData[offset + 3], renderData[offset + 4],
                                      renderData[offset + 5]);
            vertex.texCoord = glm::vec2(renderData[offset + 6], renderData[offset + 7]);

            vertices.push_back(vertex);
        }

        // Convert indices to uint16_t
        indices.reserve(indices32.size());
        for (uint32_t idx : indices32) {
            indices.push_back(static_cast<uint16_t>(idx));
        }

        std::cout << "Converted to " << vertices.size() << " vertices and " << indices.size()
                  << " indices" << std::endl;
    }

    void createBuffers() {
        auto device = engine->getDevice();

        // Create vertex buffer
        vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        vertexBuffer =
            new MoldWing::Buffer(device, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent);
        vertexBuffer->copyData(vertices.data(), vertexBufferSize);

        // Create index buffer
        vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        indexBuffer =
            new MoldWing::Buffer(device, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent);
        indexBuffer->copyData(indices.data(), indexBufferSize);

        // Create uniform buffers
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        vk::DeviceSize uniformBufferSize = sizeof(UniformBufferObject);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffers[i] = new MoldWing::Buffer(
                device, uniformBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible |
                    vk::MemoryPropertyFlagBits::eHostCoherent);
        }
    }

    void createDescriptorSets() {
        auto device = engine->getDevice();

        // Create Descriptor Set Layout
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings{};

        // Binding 0: UBO
        bindings[0].binding = 0;
        bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

        // Binding 1: Combined Image Sampler
        bindings[1].binding = 1;
        bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        descriptorSetLayout = device->getHandle().createDescriptorSetLayout(layoutInfo);

        // Create Descriptor Pool
        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
        poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        descriptorPool = device->getHandle().createDescriptorPool(poolInfo);

        // Allocate Descriptor Sets
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets = device->getHandle().allocateDescriptorSets(allocInfo);

        // Update Descriptor Sets
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            // UBO descriptor
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i]->getHandle();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            // Texture descriptor
            vk::DescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            imageInfo.imageView = textureImage->getImageView();
            imageInfo.sampler = textureSampler->getHandle();

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};

            // Write UBO
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            // Write texture sampler
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            device->getHandle().updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()),
                                                      descriptorWrites.data(), 0, nullptr);
        }
    }

    void createGraphicsPipeline() {
        MoldWing::PipelineConfig pipelineConfig{};

        // Vertex input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        pipelineConfig.vertexBindings = {bindingDescription};
        pipelineConfig.vertexAttributes = {attributeDescriptions[0], attributeDescriptions[1],
                                            attributeDescriptions[2]};

        // Descriptor Set Layout
        pipelineConfig.descriptorSetLayouts = {descriptorSetLayout};

        // Enable depth test and back-face culling
        pipelineConfig.enableDepthTest = true;
        pipelineConfig.cullMode = vk::CullModeFlagBits::eBack;

        pipeline = new MoldWing::GraphicsPipeline(
            engine->getDevice(), engine->getRenderPass()->getHandle(), Shaders::model_vert_data,
            Shaders::model_vert_size, Shaders::model_frag_data, Shaders::model_frag_size,
            engine->getSwapchain()->getExtent(), &pipelineConfig);
    }

    void updateUniformBuffer(uint32_t currentFrame) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time =
            std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime)
                .count();

        rotation = time * glm::radians(45.0f); // 45 degrees per second

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.view = glm::lookAt(glm::vec3(2.5f, 2.5f, 2.5f), glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, 1.0f, 0.0f));

        ubo.proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1; // Flip Y for Vulkan

        uniformBuffers[currentFrame]->copyData(&ubo, sizeof(ubo));
    }

    void mainLoop() {
        uint32_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            updateUniformBuffer(currentFrame);

            engine->drawFrame([this, currentFrame](vk::CommandBuffer cmd, uint32_t imageIndex) {
                vk::RenderPassBeginInfo renderPassInfo{};
                renderPassInfo.renderPass = engine->getRenderPass()->getHandle();
                renderPassInfo.framebuffer =
                    engine->getRenderPass()->getFramebuffers()[imageIndex];
                renderPassInfo.renderArea.extent = engine->getSwapchain()->getExtent();

                std::array<vk::ClearValue, 2> clearValues{};
                clearValues[0].color =
                    vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f});
                clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();

                cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->getHandle());

                vk::Buffer vertexBuffers[] = {vertexBuffer->getHandle()};
                vk::DeviceSize offsets[] = {0};
                cmd.bindVertexBuffers(0, 1, vertexBuffers, offsets);
                cmd.bindIndexBuffer(indexBuffer->getHandle(), 0, vk::IndexType::eUint16);

                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getLayout(), 0,
                                       descriptorSets[currentFrame], nullptr);

                cmd.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

                cmd.endRenderPass();
            });

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        engine->waitIdle();
    }

    void cleanup() {
        auto device = engine->getDevice();

        device->getHandle().destroyDescriptorPool(descriptorPool);
        device->getHandle().destroyDescriptorSetLayout(descriptorSetLayout);

        delete textureSampler;
        delete textureImage;

        delete pipeline;
        delete vertexBuffer;
        delete indexBuffer;

        for (auto buffer : uniformBuffers) {
            delete buffer;
        }

        delete engine;
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    ModelDemo app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
