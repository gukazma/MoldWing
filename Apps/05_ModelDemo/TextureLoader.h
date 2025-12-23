#pragma once

#include <MoldWing/VulkanEngine/Image.h>
#include <MoldWing/VulkanEngine/Device.h>
#include <MoldWing/VulkanEngine/Buffer.h>
#include <string>

namespace MoldWing {

class TextureLoader {
  public:
    static Image* loadTexture(Device* device, const std::string& filepath);
};

} // namespace MoldWing
