#include <MoldWing/VulkanEngine/Engine.h>
#include <MoldWing/VulkanEngine/GraphicsPipeline.h>
#include <MoldWing/VulkanEngine/Buffer.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Include generated shader headers
#include <MoldWing/Shaders/cube.vert.h>
#include <MoldWing/Shaders/cube.frag.h>

// Include our camera class
#include <MoldWing/VulkanEngine/Camera.h>

#include <iostream>
#include <stdexcept>
#include <array>

// Vertex structure matching shader
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Uniform Buffer Object matching shader
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Cube vertex data (8 unique vertices)
const std::vector<Vertex> cubeVertices = {
    // Front face (Red)
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},

    // Back face (Green)
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

    // Top face (Blue)
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},

    // Bottom face (Yellow)
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},

    // Right face (Magenta)
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},

    // Left face (Cyan)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}}
};

// Cube indices (36 indices for 12 triangles)
const std::vector<uint16_t> cubeIndices = {
    0,  1,  2,  2,  3,  0,   // Front
    4,  5,  6,  6,  7,  4,   // Back
    8,  9, 10, 10, 11,  8,   // Top
    12, 13, 14, 14, 15, 12,  // Bottom
    16, 17, 18, 18, 19, 16,  // Right
    20, 21, 22, 22, 23, 20   // Left
};

class CameraDemo {
public:
    void run() {
        initWindow();
        initVulkanEngine();
        createBuffers();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
        createGraphicsPipeline();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    MoldWing::Engine* engine = nullptr;
    MoldWing::GraphicsPipeline* cubePipeline = nullptr;

    MoldWing::Buffer* vertexBuffer = nullptr;
    MoldWing::Buffer* indexBuffer = nullptr;
    std::vector<MoldWing::Buffer*> uniformBuffers;

    vk::DescriptorSetLayout descriptorSetLayout;
    vk::DescriptorPool descriptorPool;
    std::vector<vk::DescriptorSet> descriptorSets;

    // Camera
    MoldWing::Camera camera;

    // Mouse state
    struct {
        bool leftButtonPressed = false;
        bool middleButtonPressed = false;
        double lastX = 0.0;
        double lastY = 0.0;
        bool firstMouse = true;
    } mouseState;

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Camera Control Demo (OSG-style) - Left: Rotate | Middle: Pan | Scroll: Zoom", nullptr, nullptr);

        // Set user pointer to this instance for callbacks
        glfwSetWindowUserPointer(window, this);

        // Setup mouse callbacks
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);

        // Initialize camera with a good viewing angle
        camera = MoldWing::Camera(
            glm::vec3(3.0f, 2.0f, 3.0f),  // position
            glm::vec3(0.0f, 0.0f, 0.0f),  // target (rotation center)
            glm::vec3(0.0f, 1.0f, 0.0f)   // up
        );
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* app = reinterpret_cast<CameraDemo*>(glfwGetWindowUserPointer(window));

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                app->mouseState.leftButtonPressed = true;
                app->mouseState.firstMouse = true;
            } else if (action == GLFW_RELEASE) {
                app->mouseState.leftButtonPressed = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                app->mouseState.middleButtonPressed = true;
                app->mouseState.firstMouse = true;
            } else if (action == GLFW_RELEASE) {
                app->mouseState.middleButtonPressed = false;
            }
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = reinterpret_cast<CameraDemo*>(glfwGetWindowUserPointer(window));

        if (app->mouseState.firstMouse) {
            app->mouseState.lastX = xpos;
            app->mouseState.lastY = ypos;
            app->mouseState.firstMouse = false;
            return; // Don't process movement on first mouse event
        }

        double xoffset = xpos - app->mouseState.lastX;
        double yoffset = ypos - app->mouseState.lastY;

        app->mouseState.lastX = xpos;
        app->mouseState.lastY = ypos;

        if (app->mouseState.leftButtonPressed) {
            // Trackball rotation - pass screen dimensions for proper normalization
            app->camera.rotate(
                static_cast<float>(xoffset),
                static_cast<float>(yoffset),
                static_cast<float>(app->WIDTH),
                static_cast<float>(app->HEIGHT)
            );
        } else if (app->mouseState.middleButtonPressed) {
            // Pan - pass screen dimensions for proper scaling
            app->camera.pan(
                static_cast<float>(xoffset),
                static_cast<float>(yoffset),
                static_cast<float>(app->WIDTH),
                static_cast<float>(app->HEIGHT)
            );
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = reinterpret_cast<CameraDemo*>(glfwGetWindowUserPointer(window));
        app->camera.zoom(static_cast<float>(yoffset));
    }

    void initVulkanEngine() {
        MoldWing::EngineConfig config;
        config.appName = "Camera Control Demo";
        config.width = WIDTH;
        config.height = HEIGHT;
        config.maxFramesInFlight = MAX_FRAMES_IN_FLIGHT;

        engine = new MoldWing::Engine(window, config);
    }

    void createBuffers() {
        // Create vertex buffer
        vertexBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            cubeVertices,
            vk::BufferUsageFlagBits::eVertexBuffer);

        // Create index buffer
        indexBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            cubeIndices,
            vk::BufferUsageFlagBits::eIndexBuffer);

        // Create uniform buffers (one per frame in flight)
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffers[i] = new MoldWing::Buffer(
                engine->getDevice(),
                sizeof(UniformBufferObject),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);
        }
    }

    void createDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        descriptorSetLayout = engine->getDevice()->getHandle().createDescriptorSetLayout(layoutInfo);
    }

    void createDescriptorPool() {
        vk::DescriptorPoolSize poolSize{};
        poolSize.type = vk::DescriptorType::eUniformBuffer;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        descriptorPool = engine->getDevice()->getHandle().createDescriptorPool(poolInfo);
    }

    void createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets = engine->getDevice()->getHandle().allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i]->getHandle();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            vk::WriteDescriptorSet descriptorWrite{};
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            engine->getDevice()->getHandle().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }
    }

    void createGraphicsPipeline() {
        MoldWing::PipelineConfig pipelineConfig{};

        // Vertex input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        pipelineConfig.vertexBindings = {bindingDescription};
        pipelineConfig.vertexAttributes = {
            attributeDescriptions[0],
            attributeDescriptions[1]
        };

        // Descriptor Set Layout
        pipelineConfig.descriptorSetLayouts = {descriptorSetLayout};

        // Enable depth test
        pipelineConfig.enableDepthTest = true;
        pipelineConfig.cullMode = vk::CullModeFlagBits::eBack;

        cubePipeline = new MoldWing::GraphicsPipeline(
            engine->getDevice(),
            engine->getRenderPass()->getHandle(),
            Shaders::cube_vert_data, Shaders::cube_vert_size,
            Shaders::cube_frag_data, Shaders::cube_frag_size,
            engine->getSwapchain()->getExtent(),
            &pipelineConfig);
    }

    void updateUniformBuffer(uint32_t currentFrame) {
        UniformBufferObject ubo{};

        // Static model (no rotation, just display the cube)
        ubo.model = glm::mat4(1.0f);

        // Use camera view matrix
        ubo.view = camera.getViewMatrix();

        // Perspective projection
        ubo.proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1; // GLM was designed for OpenGL, flip Y for Vulkan

        uniformBuffers[currentFrame]->copyData(&ubo, sizeof(ubo));
    }

    void mainLoop() {
        size_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            updateUniformBuffer(currentFrame);
            drawFrame(currentFrame);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
        engine->waitIdle();
    }

    void drawFrame(size_t currentFrame) {
        engine->drawFrame([this, currentFrame](vk::CommandBuffer cmd, uint32_t imageIndex) {
            // Begin render pass
            vk::RenderPassBeginInfo renderPassInfo{};
            renderPassInfo.renderPass = engine->getRenderPass()->getHandle();
            renderPassInfo.framebuffer = engine->getRenderPass()->getFramebuffers()[imageIndex];
            renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
            renderPassInfo.renderArea.extent = engine->getSwapchain()->getExtent();

            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color = std::array<float, 4>{0.1f, 0.1f, 0.15f, 1.0f};
            clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

            // Bind pipeline
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, cubePipeline->getHandle());

            // Bind vertex buffer
            vk::Buffer vertexBuffers[] = {vertexBuffer->getHandle()};
            vk::DeviceSize offsets[] = {0};
            cmd.bindVertexBuffers(0, 1, vertexBuffers, offsets);

            // Bind index buffer
            cmd.bindIndexBuffer(indexBuffer->getHandle(), 0, vk::IndexType::eUint16);

            // Bind descriptor set
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                cubePipeline->getLayout(),
                0, 1, &descriptorSets[currentFrame],
                0, nullptr);

            // Draw indexed
            cmd.drawIndexed(static_cast<uint32_t>(cubeIndices.size()), 1, 0, 0, 0);

            cmd.endRenderPass();
        });
    }

    void cleanup() {
        if (descriptorPool) {
            engine->getDevice()->getHandle().destroyDescriptorPool(descriptorPool);
        }
        if (descriptorSetLayout) {
            engine->getDevice()->getHandle().destroyDescriptorSetLayout(descriptorSetLayout);
        }

        for (auto* ubo : uniformBuffers) {
            delete ubo;
        }
        delete indexBuffer;
        delete vertexBuffer;
        delete cubePipeline;
        delete engine;

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    CameraDemo app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
