#pragma once

#include "Export.h"
#include "Image.h"
#include "Device.h"
#include <string>

namespace MoldWing {

/**
 * @brief Utility class for loading textures from image files
 */
class VulkanEngine_API TextureLoader {
  public:
    /**
     * @brief Load a texture from an image file using OpenCV
     * @param device Vulkan device
     * @param filepath Path to the image file
     * @return Pointer to the created Image object (caller owns the memory)
     */
    static Image* loadTexture(Device* device, const std::string& filepath);
};

} // namespace MoldWing
