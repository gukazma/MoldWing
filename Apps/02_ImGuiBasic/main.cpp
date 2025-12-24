#include <MoldWing/VulkanEngine/Engine.h>
#include <MoldWing/VulkanEngine/GraphicsPipeline.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

// Include generated shader headers
#include <MoldWing/Shaders/shader.vert.h>
#include <MoldWing/Shaders/shader.frag.h>

#include <iostream>
#include <stdexcept>

// Minimal Vulkan Demo with VulkanEngine library + ImGui

class VulkanDemo {
public:
    void run() {
        initWindow();
        initVulkanEngine();
        initGraphicsPipeline();
        initImGui();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    MoldWing::Engine* engine = nullptr;
    MoldWing::GraphicsPipeline* graphicsPipeline = nullptr;
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan ImGui Demo (VulkanEngine)", nullptr, nullptr);
    }

    void initVulkanEngine() {
        MoldWing::EngineConfig config;
        config.appName = "Vulkan ImGui Demo";
        config.width = WIDTH;
        config.height = HEIGHT;
        config.maxFramesInFlight = 2;

        engine = new MoldWing::Engine(window, config);
    }

    void initGraphicsPipeline() {
        // Create graphics pipeline using embedded shaders
        graphicsPipeline = new MoldWing::GraphicsPipeline(
            engine->getDevice(),
            engine->getRenderPass()->getHandle(),
            Shaders::shader_vert_data, Shaders::shader_vert_size,
            Shaders::shader_frag_data, Shaders::shader_frag_size,
            engine->getSwapchain()->getExtent()
        );
    }

    void initImGui() {
        // Create descriptor pool for ImGui
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        auto vkDevice = static_cast<VkDevice>(engine->getDevice()->getHandle());
        if (vkCreateDescriptorPool(vkDevice, &pool_info, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ImGui descriptor pool");
        }

        // Setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = static_cast<VkInstance>(engine->getInstance()->getHandle());
        init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(engine->getDevice()->getPhysicalDevice());
        init_info.Device = static_cast<VkDevice>(engine->getDevice()->getHandle());
        init_info.QueueFamily = engine->getDevice()->getQueueFamilyIndices().graphicsFamily.value();
        init_info.Queue = static_cast<VkQueue>(engine->getDevice()->getGraphicsQueue());
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = imguiDescriptorPool;
        init_info.RenderPass = static_cast<VkRenderPass>(engine->getRenderPass()->getHandle());
        init_info.Subpass = 0;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(engine->getSwapchain()->getImages().size());
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);

        // Upload fonts
        ImGui_ImplVulkan_CreateFontsTexture();
        ImGui_ImplVulkan_DestroyFontsTexture();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        engine->waitIdle();
    }

    void drawFrame() {
        // Start ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Demo ImGui window
        {
            static float f = 0.0f;
            static int counter = 0;
            static float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};

            ImGui::Begin("Vulkan + ImGui Demo (VulkanEngine)");
            ImGui::Text("This demo uses the VulkanEngine library!");
            ImGui::Separator();

            ImGui::SliderFloat("Float slider", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("Clear color", clearColor);

            if (ImGui::Button("Click me!"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::End();
        }

        ImGui::Render();

        // Record and submit using VulkanEngine
        engine->drawFrame([this](vk::CommandBuffer cmd, uint32_t imageIndex) {
            // Begin render pass
            vk::RenderPassBeginInfo renderPassInfo{};
            renderPassInfo.renderPass = engine->getRenderPass()->getHandle();
            renderPassInfo.framebuffer = engine->getRenderPass()->getFramebuffers()[imageIndex];
            renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
            renderPassInfo.renderArea.extent = engine->getSwapchain()->getExtent();

            vk::ClearValue clearColor{std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

            // Draw triangle
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getHandle());
            cmd.draw(3, 1, 0, 0);

            // Render ImGui on top
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));

            cmd.endRenderPass();
        });
    }

    void cleanup() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (imguiDescriptorPool && engine) {
            auto vkDevice = static_cast<VkDevice>(engine->getDevice()->getHandle());
            vkDestroyDescriptorPool(vkDevice, imguiDescriptorPool, nullptr);
        }

        delete graphicsPipeline;
        delete engine;

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    VulkanDemo app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
