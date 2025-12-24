// Enable dynamic dispatch loader for KHR extension functions
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

// Define storage for the dynamic dispatcher
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <MoldWing/VulkanEngine/Engine.h>
#include <MoldWing/VulkanEngine/GraphicsPipeline.h>
#include <MoldWing/VulkanEngine/Buffer.h>
#include <MoldWing/VulkanEngine/Camera.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// Include generated shader headers
#include <MoldWing/Shaders/cube_instanced.vert.h>
#include <MoldWing/Shaders/cube_instanced.frag.h>
#include <MoldWing/Shaders/raypick.comp.h>

#include <iostream>
#include <stdexcept>
#include <array>
#include <chrono>

// Vertex structure
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
        std::array<vk::VertexInputAttributeDescription, 2> attrs{};
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = vk::Format::eR32G32B32Sfloat;
        attrs[0].offset = offsetof(Vertex, pos);

        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = vk::Format::eR32G32B32Sfloat;
        attrs[1].offset = offsetof(Vertex, color);
        return attrs;
    }
};

// Instance data for each cube
struct InstanceData {
    glm::mat4 model;
    glm::vec4 color;  // w = highlight intensity (0 = use vertex color)

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 1;
        bindingDescription.stride = sizeof(InstanceData);
        bindingDescription.inputRate = vk::VertexInputRate::eInstance;
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 5> attrs{};
        // mat4 takes 4 locations (2, 3, 4, 5)
        for (int i = 0; i < 4; i++) {
            attrs[i].binding = 1;
            attrs[i].location = 2 + i;
            attrs[i].format = vk::Format::eR32G32B32A32Sfloat;
            attrs[i].offset = sizeof(glm::vec4) * i;
        }
        // vec4 color at location 6
        attrs[4].binding = 1;
        attrs[4].location = 6;
        attrs[4].format = vk::Format::eR32G32B32A32Sfloat;
        attrs[4].offset = offsetof(InstanceData, color);
        return attrs;
    }
};

// Uniform Buffer Object
struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Ray parameters for compute shader
struct RayParams {
    alignas(16) glm::vec4 origin;     // xyz = origin, w = tMin
    alignas(16) glm::vec4 direction;  // xyz = direction, w = tMax
};

// Hit result from compute shader
struct HitResult {
    int32_t hit;
    int32_t instanceId;
    int32_t primitiveId;
    float hitT;
    glm::vec4 hitPoint;
    glm::vec4 barycentrics;
};

// Ray picking state
struct RayPickState {
    // Real-time tracking
    glm::vec3 currentHitPoint{0.0f};
    glm::vec3 currentRayOrigin{0.0f};
    glm::vec3 currentRayDirection{0.0f};
    int currentInstanceId = -1;
    bool isHit = false;

    // Fixed point (right-click)
    glm::vec3 fixedHitPoint{0.0f};
    int fixedInstanceId = -1;
    bool hasFixedPoint = false;

    // Performance
    double queryTimeMs = 0.0;
    double fps = 0.0;
};

// Cube vertices (same as 04_CameraControl)
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

const std::vector<uint32_t> cubeIndices = {
    0,  1,  2,  2,  3,  0,   // Front
    4,  5,  6,  6,  7,  4,   // Back
    8,  9, 10, 10, 11,  8,   // Top
    12, 13, 14, 14, 15, 12,  // Bottom
    16, 17, 18, 18, 19, 16,  // Right
    20, 21, 22, 22, 23, 20   // Left
};

class RayPickDemo {
public:
    void run() {
        initWindow();
        initVulkanEngine();
        createBuffers();
        createAccelerationStructures();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
        createGraphicsPipeline();
        createComputePipeline();
        initImGui();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    MoldWing::Engine* engine = nullptr;
    MoldWing::GraphicsPipeline* cubePipeline = nullptr;

    // Buffers
    MoldWing::Buffer* vertexBuffer = nullptr;
    MoldWing::Buffer* indexBuffer = nullptr;
    MoldWing::Buffer* instanceBuffer = nullptr;
    std::vector<MoldWing::Buffer*> uniformBuffers;

    // Ray tracing buffers
    MoldWing::Buffer* rayParamsBuffer = nullptr;
    MoldWing::Buffer* hitResultBuffer = nullptr;

    // Acceleration structures
    vk::AccelerationStructureKHR blas;
    vk::AccelerationStructureKHR tlas;
    MoldWing::Buffer* blasBuffer = nullptr;
    MoldWing::Buffer* tlasBuffer = nullptr;
    MoldWing::Buffer* instancesBuffer = nullptr;  // For TLAS instances

    // Descriptors
    vk::DescriptorSetLayout descriptorSetLayout;
    vk::DescriptorSetLayout computeDescriptorSetLayout;
    vk::DescriptorPool descriptorPool;
    std::vector<vk::DescriptorSet> descriptorSets;
    vk::DescriptorSet computeDescriptorSet;

    // Compute pipeline
    vk::Pipeline computePipeline;
    vk::PipelineLayout computePipelineLayout;

    // ImGui
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    // Camera
    MoldWing::Camera camera;

    // Instance data for 27 cubes
    std::vector<InstanceData> instances;

    // Ray picking state
    RayPickState pickState;

    // Mouse state
    struct {
        bool leftButtonPressed = false;
        bool middleButtonPressed = false;
        double lastX = 0.0;
        double lastY = 0.0;
        double currentX = 0.0;
        double currentY = 0.0;
        bool firstMouse = true;
    } mouseState;

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    const int CUBE_GRID_SIZE = 3;  // 3x3x3 = 27 cubes
    const float CUBE_SPACING = 1.5f;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT,
            "Ray Picking Demo (RTX) - Left: Rotate | Middle: Pan | Scroll: Zoom | Right: Fix Point",
            nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);

        // Initialize camera
        camera = MoldWing::Camera(
            glm::vec3(6.0f, 4.0f, 6.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* app = reinterpret_cast<RayPickDemo*>(glfwGetWindowUserPointer(window));

        // Let ImGui handle mouse if it wants to
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) return;

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
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
            // Fix current hit point
            if (app->pickState.isHit) {
                app->pickState.fixedHitPoint = app->pickState.currentHitPoint;
                app->pickState.fixedInstanceId = app->pickState.currentInstanceId;
                app->pickState.hasFixedPoint = true;
            }
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = reinterpret_cast<RayPickDemo*>(glfwGetWindowUserPointer(window));

        app->mouseState.currentX = xpos;
        app->mouseState.currentY = ypos;

        if (app->mouseState.firstMouse) {
            app->mouseState.lastX = xpos;
            app->mouseState.lastY = ypos;
            app->mouseState.firstMouse = false;
            return;
        }

        double xoffset = xpos - app->mouseState.lastX;
        double yoffset = ypos - app->mouseState.lastY;

        app->mouseState.lastX = xpos;
        app->mouseState.lastY = ypos;

        // Let ImGui handle mouse if it wants to
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) return;

        if (app->mouseState.leftButtonPressed) {
            app->camera.rotate(
                static_cast<float>(xoffset),
                static_cast<float>(yoffset),
                static_cast<float>(app->WIDTH),
                static_cast<float>(app->HEIGHT)
            );
        } else if (app->mouseState.middleButtonPressed) {
            app->camera.pan(
                static_cast<float>(xoffset),
                static_cast<float>(yoffset),
                static_cast<float>(app->WIDTH),
                static_cast<float>(app->HEIGHT)
            );
        }
    }

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = reinterpret_cast<RayPickDemo*>(glfwGetWindowUserPointer(window));

        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) return;

        app->camera.zoom(static_cast<float>(yoffset));
    }

    void initVulkanEngine() {
        // Initialize dynamic dispatcher with base functions
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        MoldWing::EngineConfig config;
        config.appName = "Ray Picking Demo";
        config.width = WIDTH;
        config.height = HEIGHT;
        config.maxFramesInFlight = MAX_FRAMES_IN_FLIGHT;
        config.enableRayTracing = true;  // Enable ray tracing extensions

        engine = new MoldWing::Engine(window, config);

        // Initialize dispatcher with instance and device functions
        VULKAN_HPP_DEFAULT_DISPATCHER.init(engine->getInstance()->getHandle());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(engine->getDevice()->getHandle());
    }

    void createBuffers() {
        // Create vertex buffer with acceleration structure build flag
        vertexBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            cubeVertices,
            vk::BufferUsageFlagBits::eVertexBuffer |
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress);

        // Create index buffer
        indexBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            cubeIndices,
            vk::BufferUsageFlagBits::eIndexBuffer |
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress);

        // Generate 3x3x3 cube instances
        instances.resize(CUBE_GRID_SIZE * CUBE_GRID_SIZE * CUBE_GRID_SIZE);
        int idx = 0;
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    glm::vec3 pos(x * CUBE_SPACING, y * CUBE_SPACING, z * CUBE_SPACING);
                    instances[idx].model = glm::translate(glm::mat4(1.0f), pos);
                    instances[idx].color = glm::vec4(0.0f);  // No highlight
                    idx++;
                }
            }
        }

        // Create instance buffer
        instanceBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            instances,
            vk::BufferUsageFlagBits::eVertexBuffer);

        // Create uniform buffers
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffers[i] = new MoldWing::Buffer(
                engine->getDevice(),
                sizeof(UniformBufferObject),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        // Create ray params buffer
        rayParamsBuffer = new MoldWing::Buffer(
            engine->getDevice(),
            sizeof(RayParams),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

        // Create hit result buffer
        hitResultBuffer = new MoldWing::Buffer(
            engine->getDevice(),
            sizeof(HitResult),
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void createAccelerationStructures() {
        auto device = engine->getDevice()->getHandle();

        // Get buffer device addresses
        vk::BufferDeviceAddressInfo vertexAddrInfo{};
        vertexAddrInfo.buffer = vertexBuffer->getHandle();
        vk::DeviceAddress vertexAddr = device.getBufferAddress(vertexAddrInfo);

        vk::BufferDeviceAddressInfo indexAddrInfo{};
        indexAddrInfo.buffer = indexBuffer->getHandle();
        vk::DeviceAddress indexAddr = device.getBufferAddress(indexAddrInfo);

        // === Create BLAS ===
        vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
        triangles.vertexData.deviceAddress = vertexAddr;
        triangles.vertexStride = sizeof(Vertex);
        triangles.maxVertex = static_cast<uint32_t>(cubeVertices.size() - 1);
        triangles.indexType = vk::IndexType::eUint32;
        triangles.indexData.deviceAddress = indexAddr;

        vk::AccelerationStructureGeometryKHR geometry{};
        geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
        geometry.geometry.triangles = triangles;
        geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

        vk::AccelerationStructureBuildGeometryInfoKHR blasBuildInfo{};
        blasBuildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        blasBuildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        blasBuildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        blasBuildInfo.geometryCount = 1;
        blasBuildInfo.pGeometries = &geometry;

        uint32_t primitiveCount = static_cast<uint32_t>(cubeIndices.size() / 3);

        vk::AccelerationStructureBuildSizesInfoKHR blasSizeInfo =
            device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                blasBuildInfo,
                primitiveCount);

        // Create BLAS buffer
        blasBuffer = new MoldWing::Buffer(
            engine->getDevice(),
            blasSizeInfo.accelerationStructureSize,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Create BLAS
        vk::AccelerationStructureCreateInfoKHR blasCreateInfo{};
        blasCreateInfo.buffer = blasBuffer->getHandle();
        blasCreateInfo.size = blasSizeInfo.accelerationStructureSize;
        blasCreateInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        blas = device.createAccelerationStructureKHR(blasCreateInfo);

        // Create scratch buffer for BLAS build
        auto blasScratch = std::make_unique<MoldWing::Buffer>(
            engine->getDevice(),
            blasSizeInfo.buildScratchSize,
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::BufferDeviceAddressInfo scratchAddrInfo{};
        scratchAddrInfo.buffer = blasScratch->getHandle();
        vk::DeviceAddress scratchAddr = device.getBufferAddress(scratchAddrInfo);

        // Build BLAS
        blasBuildInfo.dstAccelerationStructure = blas;
        blasBuildInfo.scratchData.deviceAddress = scratchAddr;

        vk::AccelerationStructureBuildRangeInfoKHR blasRange{};
        blasRange.primitiveCount = primitiveCount;

        auto cmdBuffer = engine->getCommandBufferManager()->beginSingleTimeCommands();
        const vk::AccelerationStructureBuildRangeInfoKHR* pBlasRange = &blasRange;
        cmdBuffer.buildAccelerationStructuresKHR(1, &blasBuildInfo, &pBlasRange);
        engine->getCommandBufferManager()->endSingleTimeCommands(cmdBuffer);

        // === Create TLAS ===
        vk::AccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
        blasAddrInfo.accelerationStructure = blas;
        vk::DeviceAddress blasAddr = device.getAccelerationStructureAddressKHR(blasAddrInfo);

        // Create instance buffer for TLAS
        std::vector<vk::AccelerationStructureInstanceKHR> asInstances(instances.size());
        for (size_t i = 0; i < instances.size(); i++) {
            // Convert glm::mat4 to VkTransformMatrixKHR (3x4 row-major)
            glm::mat4 transposed = glm::transpose(instances[i].model);
            memcpy(&asInstances[i].transform, &transposed, sizeof(vk::TransformMatrixKHR));
            asInstances[i].instanceCustomIndex = static_cast<uint32_t>(i);
            asInstances[i].mask = 0xFF;
            asInstances[i].instanceShaderBindingTableRecordOffset = 0;
            asInstances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            asInstances[i].accelerationStructureReference = blasAddr;
        }

        instancesBuffer = MoldWing::Buffer::createWithData(
            engine->getDevice(),
            asInstances,
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress);

        vk::BufferDeviceAddressInfo instancesAddrInfo{};
        instancesAddrInfo.buffer = instancesBuffer->getHandle();
        vk::DeviceAddress instancesAddr = device.getBufferAddress(instancesAddrInfo);

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
        instancesData.arrayOfPointers = VK_FALSE;
        instancesData.data.deviceAddress = instancesAddr;

        vk::AccelerationStructureGeometryKHR tlasGeometry{};
        tlasGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
        tlasGeometry.geometry.instances = instancesData;

        vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
        tlasBuildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        tlasBuildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        tlasBuildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        tlasBuildInfo.geometryCount = 1;
        tlasBuildInfo.pGeometries = &tlasGeometry;

        uint32_t instanceCount = static_cast<uint32_t>(instances.size());

        vk::AccelerationStructureBuildSizesInfoKHR tlasSizeInfo =
            device.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                tlasBuildInfo,
                instanceCount);

        // Create TLAS buffer
        tlasBuffer = new MoldWing::Buffer(
            engine->getDevice(),
            tlasSizeInfo.accelerationStructureSize,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Create TLAS
        vk::AccelerationStructureCreateInfoKHR tlasCreateInfo{};
        tlasCreateInfo.buffer = tlasBuffer->getHandle();
        tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
        tlasCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        tlas = device.createAccelerationStructureKHR(tlasCreateInfo);

        // Create scratch buffer for TLAS build
        auto tlasScratch = std::make_unique<MoldWing::Buffer>(
            engine->getDevice(),
            tlasSizeInfo.buildScratchSize,
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        scratchAddrInfo.buffer = tlasScratch->getHandle();
        scratchAddr = device.getBufferAddress(scratchAddrInfo);

        // Build TLAS
        tlasBuildInfo.dstAccelerationStructure = tlas;
        tlasBuildInfo.scratchData.deviceAddress = scratchAddr;

        vk::AccelerationStructureBuildRangeInfoKHR tlasRange{};
        tlasRange.primitiveCount = instanceCount;

        cmdBuffer = engine->getCommandBufferManager()->beginSingleTimeCommands();
        const vk::AccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;
        cmdBuffer.buildAccelerationStructuresKHR(1, &tlasBuildInfo, &pTlasRange);
        engine->getCommandBufferManager()->endSingleTimeCommands(cmdBuffer);
    }

    void createDescriptorSetLayout() {
        // Graphics descriptor set layout (UBO)
        vk::DescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboBinding;
        descriptorSetLayout = engine->getDevice()->getHandle().createDescriptorSetLayout(layoutInfo);

        // Compute descriptor set layout (TLAS + RayParams + HitResult)
        std::array<vk::DescriptorSetLayoutBinding, 3> computeBindings{};

        // TLAS
        computeBindings[0].binding = 0;
        computeBindings[0].descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        computeBindings[0].descriptorCount = 1;
        computeBindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

        // Ray params
        computeBindings[1].binding = 1;
        computeBindings[1].descriptorType = vk::DescriptorType::eUniformBuffer;
        computeBindings[1].descriptorCount = 1;
        computeBindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

        // Hit result
        computeBindings[2].binding = 2;
        computeBindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        computeBindings[2].descriptorCount = 1;
        computeBindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

        vk::DescriptorSetLayoutCreateInfo computeLayoutInfo{};
        computeLayoutInfo.bindingCount = static_cast<uint32_t>(computeBindings.size());
        computeLayoutInfo.pBindings = computeBindings.data();
        computeDescriptorSetLayout = engine->getDevice()->getHandle().createDescriptorSetLayout(computeLayoutInfo);
    }

    void createDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 4> poolSizes{};
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT + 1;  // UBO + RayParams
        poolSizes[1].type = vk::DescriptorType::eAccelerationStructureKHR;
        poolSizes[1].descriptorCount = 1;
        poolSizes[2].type = vk::DescriptorType::eStorageBuffer;
        poolSizes[2].descriptorCount = 1;
        poolSizes[3].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[3].descriptorCount = 1000;  // For ImGui

        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT + 2 + 1000;  // Graphics + Compute + ImGui
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        descriptorPool = engine->getDevice()->getHandle().createDescriptorPool(poolInfo);
    }

    void createDescriptorSets() {
        auto device = engine->getDevice()->getHandle();

        // Allocate graphics descriptor sets
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();
        descriptorSets = device.allocateDescriptorSets(allocInfo);

        // Update graphics descriptor sets
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i]->getHandle();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            vk::WriteDescriptorSet descriptorWrite{};
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }

        // Allocate compute descriptor set
        vk::DescriptorSetAllocateInfo computeAllocInfo{};
        computeAllocInfo.descriptorPool = descriptorPool;
        computeAllocInfo.descriptorSetCount = 1;
        computeAllocInfo.pSetLayouts = &computeDescriptorSetLayout;
        computeDescriptorSet = device.allocateDescriptorSets(computeAllocInfo)[0];

        // Update compute descriptor set
        vk::WriteDescriptorSetAccelerationStructureKHR asWrite{};
        asWrite.accelerationStructureCount = 1;
        asWrite.pAccelerationStructures = &tlas;

        vk::DescriptorBufferInfo rayParamsInfo{};
        rayParamsInfo.buffer = rayParamsBuffer->getHandle();
        rayParamsInfo.offset = 0;
        rayParamsInfo.range = sizeof(RayParams);

        vk::DescriptorBufferInfo hitResultInfo{};
        hitResultInfo.buffer = hitResultBuffer->getHandle();
        hitResultInfo.offset = 0;
        hitResultInfo.range = sizeof(HitResult);

        std::array<vk::WriteDescriptorSet, 3> computeWrites{};

        // TLAS
        computeWrites[0].dstSet = computeDescriptorSet;
        computeWrites[0].dstBinding = 0;
        computeWrites[0].descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        computeWrites[0].descriptorCount = 1;
        computeWrites[0].pNext = &asWrite;

        // Ray params
        computeWrites[1].dstSet = computeDescriptorSet;
        computeWrites[1].dstBinding = 1;
        computeWrites[1].descriptorType = vk::DescriptorType::eUniformBuffer;
        computeWrites[1].descriptorCount = 1;
        computeWrites[1].pBufferInfo = &rayParamsInfo;

        // Hit result
        computeWrites[2].dstSet = computeDescriptorSet;
        computeWrites[2].dstBinding = 2;
        computeWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        computeWrites[2].descriptorCount = 1;
        computeWrites[2].pBufferInfo = &hitResultInfo;

        device.updateDescriptorSets(computeWrites, {});
    }

    void createGraphicsPipeline() {
        MoldWing::PipelineConfig pipelineConfig{};

        // Vertex input (two bindings: vertices and instances)
        auto vertexBinding = Vertex::getBindingDescription();
        auto instanceBinding = InstanceData::getBindingDescription();
        pipelineConfig.vertexBindings = {vertexBinding, instanceBinding};

        auto vertexAttrs = Vertex::getAttributeDescriptions();
        auto instanceAttrs = InstanceData::getAttributeDescriptions();

        std::vector<vk::VertexInputAttributeDescription> allAttrs;
        allAttrs.insert(allAttrs.end(), vertexAttrs.begin(), vertexAttrs.end());
        allAttrs.insert(allAttrs.end(), instanceAttrs.begin(), instanceAttrs.end());
        pipelineConfig.vertexAttributes = allAttrs;

        pipelineConfig.descriptorSetLayouts = {descriptorSetLayout};
        pipelineConfig.enableDepthTest = true;
        pipelineConfig.cullMode = vk::CullModeFlagBits::eBack;

        cubePipeline = new MoldWing::GraphicsPipeline(
            engine->getDevice(),
            engine->getRenderPass()->getHandle(),
            Shaders::cube_instanced_vert_data, Shaders::cube_instanced_vert_size,
            Shaders::cube_instanced_frag_data, Shaders::cube_instanced_frag_size,
            engine->getSwapchain()->getExtent(),
            &pipelineConfig);
    }

    void createComputePipeline() {
        auto device = engine->getDevice()->getHandle();

        // Create shader module
        vk::ShaderModuleCreateInfo shaderInfo{};
        shaderInfo.codeSize = Shaders::raypick_comp_size;
        shaderInfo.pCode = reinterpret_cast<const uint32_t*>(Shaders::raypick_comp_data);
        vk::ShaderModule computeShader = device.createShaderModule(shaderInfo);

        vk::PipelineShaderStageCreateInfo stageInfo{};
        stageInfo.stage = vk::ShaderStageFlagBits::eCompute;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        // Pipeline layout
        vk::PipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        computePipelineLayout = device.createPipelineLayout(layoutInfo);

        // Create pipeline
        vk::ComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = computePipelineLayout;
        computePipeline = device.createComputePipeline(nullptr, pipelineInfo).value;

        device.destroyShaderModule(computeShader);
    }

    void initImGui() {
        // Create descriptor pool for ImGui
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;

        auto vkDevice = static_cast<VkDevice>(engine->getDevice()->getHandle());
        vkCreateDescriptorPool(vkDevice, &pool_info, nullptr, &imguiDescriptorPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = static_cast<VkInstance>(engine->getInstance()->getHandle());
        init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(engine->getDevice()->getPhysicalDevice());
        init_info.Device = vkDevice;
        init_info.QueueFamily = engine->getDevice()->getQueueFamilyIndices().graphicsFamily.value();
        init_info.Queue = static_cast<VkQueue>(engine->getDevice()->getGraphicsQueue());
        init_info.DescriptorPool = imguiDescriptorPool;
        init_info.RenderPass = static_cast<VkRenderPass>(engine->getRenderPass()->getHandle());
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(engine->getSwapchain()->getImages().size());
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);
        ImGui_ImplVulkan_CreateFontsTexture();
    }

    glm::vec3 computeRayDirection(float mouseX, float mouseY) {
        // Screen -> NDC (standard OpenGL convention)
        float ndcX = (2.0f * mouseX / WIDTH) - 1.0f;
        float ndcY = 1.0f - (2.0f * mouseY / HEIGHT);  // Flip Y for screen coords

        // Get matrices - use standard projection (NO Vulkan Y flip for ray calculation)
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        // Note: Do NOT apply Vulkan Y flip here, it's only for rendering
        glm::mat4 view = camera.getViewMatrix();

        glm::mat4 invProj = glm::inverse(proj);
        glm::mat4 invView = glm::inverse(view);

        // NDC -> View space
        glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 rayEye = invProj * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        // View -> World space
        glm::vec3 rayWorld = glm::normalize(glm::vec3(invView * rayEye));
        return rayWorld;
    }

    void performRayQuery() {
        auto start = std::chrono::high_resolution_clock::now();

        // Update ray parameters
        glm::vec3 rayOrigin = camera.getPosition();
        glm::vec3 rayDir = computeRayDirection(
            static_cast<float>(mouseState.currentX),
            static_cast<float>(mouseState.currentY));

        RayParams params;
        params.origin = glm::vec4(rayOrigin, 0.001f);  // w = tMin
        params.direction = glm::vec4(rayDir, 1000.0f);  // w = tMax

        rayParamsBuffer->copyData(&params, sizeof(params));

        pickState.currentRayOrigin = rayOrigin;
        pickState.currentRayDirection = rayDir;

        // Execute compute shader
        auto cmdBuffer = engine->getCommandBufferManager()->beginSingleTimeCommands();

        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
        cmdBuffer.dispatch(1, 1, 1);

        // Memory barrier
        vk::MemoryBarrier barrier{};
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        cmdBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eHost,
            {}, barrier, {}, {});

        engine->getCommandBufferManager()->endSingleTimeCommands(cmdBuffer);

        // Read result
        HitResult result;
        hitResultBuffer->copyToHost(&result, sizeof(result));

        pickState.isHit = result.hit != 0;
        if (pickState.isHit) {
            pickState.currentHitPoint = glm::vec3(result.hitPoint);
            pickState.currentInstanceId = result.instanceId;
        } else {
            pickState.currentInstanceId = -1;
        }

        auto end = std::chrono::high_resolution_clock::now();
        pickState.queryTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    }

    void updateInstanceHighlights() {
        // Reset all highlights
        for (auto& inst : instances) {
            inst.color = glm::vec4(0.0f);
        }

        // Highlight current (yellow)
        if (pickState.isHit && pickState.currentInstanceId >= 0 &&
            pickState.currentInstanceId < static_cast<int>(instances.size())) {
            instances[pickState.currentInstanceId].color = glm::vec4(1.0f, 1.0f, 0.0f, 0.5f);
        }

        // Highlight fixed (green, override current)
        if (pickState.hasFixedPoint && pickState.fixedInstanceId >= 0 &&
            pickState.fixedInstanceId < static_cast<int>(instances.size())) {
            instances[pickState.fixedInstanceId].color = glm::vec4(0.0f, 1.0f, 0.0f, 0.7f);
        }

        // Update instance buffer
        instanceBuffer->copyData(instances.data(), instances.size() * sizeof(InstanceData));
    }

    void updateUniformBuffer(uint32_t currentFrame) {
        UniformBufferObject ubo{};
        ubo.view = camera.getViewMatrix();
        ubo.proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1;
        uniformBuffers[currentFrame]->copyData(&ubo, sizeof(ubo));
    }

    void drawImGui() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Ray Picking Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // Hit Status
        if (ImGui::CollapsingHeader("Hit Status", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (pickState.isHit) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Hit: YES");
                ImGui::Text("Instance ID: %d", pickState.currentInstanceId);
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Hit: NO");
                ImGui::Text("Instance ID: -");
            }
        }

        // Hit Point
        if (ImGui::CollapsingHeader("Hit Point", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (pickState.isHit) {
                ImGui::Text("Position: (%.3f, %.3f, %.3f)",
                    pickState.currentHitPoint.x,
                    pickState.currentHitPoint.y,
                    pickState.currentHitPoint.z);
                float distance = glm::length(pickState.currentHitPoint - pickState.currentRayOrigin);
                ImGui::Text("Distance: %.3f", distance);
            } else {
                ImGui::Text("Position: -");
                ImGui::Text("Distance: -");
            }
        }

        // Ray Info
        if (ImGui::CollapsingHeader("Ray Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Origin: (%.2f, %.2f, %.2f)",
                pickState.currentRayOrigin.x,
                pickState.currentRayOrigin.y,
                pickState.currentRayOrigin.z);
            ImGui::Text("Direction: (%.3f, %.3f, %.3f)",
                pickState.currentRayDirection.x,
                pickState.currentRayDirection.y,
                pickState.currentRayDirection.z);
        }

        // Performance
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Query Time: %.3f ms", pickState.queryTimeMs);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        }

        // Camera
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 pos = camera.getPosition();
            glm::vec3 center = camera.getCenter();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
            ImGui::Text("Center: (%.2f, %.2f, %.2f)", center.x, center.y, center.z);
            ImGui::Text("Distance: %.2f", camera.getDistance());
        }

        // Fixed Point
        if (ImGui::CollapsingHeader("Fixed Point (Right-click)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (pickState.hasFixedPoint) {
                ImGui::Text("Instance ID: %d", pickState.fixedInstanceId);
                ImGui::Text("Position: (%.3f, %.3f, %.3f)",
                    pickState.fixedHitPoint.x,
                    pickState.fixedHitPoint.y,
                    pickState.fixedHitPoint.z);
                if (ImGui::Button("Clear")) {
                    pickState.hasFixedPoint = false;
                    pickState.fixedInstanceId = -1;
                }
            } else {
                ImGui::TextDisabled("No fixed point set");
                ImGui::TextDisabled("Right-click on a cube to fix");
            }
        }

        ImGui::End();
        ImGui::Render();
    }

    void mainLoop() {
        size_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // Perform ray query
            performRayQuery();

            // Update highlights
            updateInstanceHighlights();

            // Update UBO
            updateUniformBuffer(currentFrame);

            // Draw ImGui
            drawImGui();

            // Draw frame
            drawFrame(currentFrame);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
        engine->waitIdle();
    }

    void drawFrame(size_t currentFrame) {
        engine->drawFrame([this, currentFrame](vk::CommandBuffer cmd, uint32_t imageIndex) {
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

            // Bind vertex buffers (vertices + instances)
            vk::Buffer vertexBuffers[] = {vertexBuffer->getHandle(), instanceBuffer->getHandle()};
            vk::DeviceSize offsets[] = {0, 0};
            cmd.bindVertexBuffers(0, 2, vertexBuffers, offsets);

            // Bind index buffer
            cmd.bindIndexBuffer(indexBuffer->getHandle(), 0, vk::IndexType::eUint32);

            // Bind descriptor set
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                cubePipeline->getLayout(),
                0, 1, &descriptorSets[currentFrame],
                0, nullptr);

            // Draw all instances
            cmd.drawIndexed(
                static_cast<uint32_t>(cubeIndices.size()),
                static_cast<uint32_t>(instances.size()),
                0, 0, 0);

            // Draw ImGui
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));

            cmd.endRenderPass();
        });
    }

    void cleanup() {
        auto device = engine->getDevice()->getHandle();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (imguiDescriptorPool) {
            vkDestroyDescriptorPool(static_cast<VkDevice>(device), imguiDescriptorPool, nullptr);
        }

        device.destroyPipeline(computePipeline);
        device.destroyPipelineLayout(computePipelineLayout);

        device.destroyAccelerationStructureKHR(tlas);
        device.destroyAccelerationStructureKHR(blas);

        delete tlasBuffer;
        delete blasBuffer;
        delete instancesBuffer;

        device.destroyDescriptorPool(descriptorPool);
        device.destroyDescriptorSetLayout(descriptorSetLayout);
        device.destroyDescriptorSetLayout(computeDescriptorSetLayout);

        delete hitResultBuffer;
        delete rayParamsBuffer;

        for (auto* ubo : uniformBuffers) {
            delete ubo;
        }
        delete instanceBuffer;
        delete indexBuffer;
        delete vertexBuffer;

        delete cubePipeline;
        delete engine;

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    RayPickDemo app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
