#pragma once
#include "Export.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>

namespace VulkanEngine {

class VulkanEngine_API Instance {
public:
    Instance(const std::string& appName, uint32_t appVersion,
             const std::vector<const char*>& extensions = {},
             const std::vector<const char*>& layers = {});
    ~Instance();

    vk::Instance getHandle() const { return instance; }
    operator vk::Instance() const { return instance; }

private:
    vk::Instance instance;
};

} // namespace VulkanEngine
