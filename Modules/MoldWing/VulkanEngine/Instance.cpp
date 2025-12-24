#include "Instance.h"
#include <stdexcept>

namespace MoldWing {

Instance::Instance(const std::string& appName, uint32_t appVersion,
                   const std::vector<const char*>& extensions,
                   const std::vector<const char*>& layers) {
    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = "VulkanEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;  // Required for ray tracing features

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    instance = vk::createInstance(createInfo);
    if (!instance) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

Instance::~Instance() {
    if (instance) {
        instance.destroy();
    }
}

} // namespace MoldWing
