#include "TextureLoader.h"
#include "Buffer.h"
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <iostream>

namespace MoldWing {

Image* TextureLoader::loadTexture(Device* device, const std::string& filepath) {
    // Load image using OpenCV
    cv::Mat image = cv::imread(filepath, cv::IMREAD_COLOR);
    if (image.empty()) {
        throw std::runtime_error("Failed to load texture: " + filepath);
    }

    // Convert BGR to RGBA
    cv::Mat rgbaImage;
    cv::cvtColor(image, rgbaImage, cv::COLOR_BGR2RGBA);

    std::cout << "Loaded texture: " << filepath << std::endl;
    std::cout << "  Size: " << rgbaImage.cols << "x" << rgbaImage.rows << std::endl;
    std::cout << "  Channels: " << rgbaImage.channels() << std::endl;

    uint32_t width = rgbaImage.cols;
    uint32_t height = rgbaImage.rows;
    vk::DeviceSize imageSize = width * height * 4; // RGBA

    // Create staging buffer
    Buffer stagingBuffer(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent);

    // Copy image data to staging buffer
    stagingBuffer.copyData(rgbaImage.data, imageSize);

    // Create texture image
    Image* textureImage =
        new Image(device, width, height, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Transition image layout and copy from buffer
    textureImage->transitionLayout(vk::ImageLayout::eUndefined,
                                    vk::ImageLayout::eTransferDstOptimal);
    textureImage->copyFromBuffer(stagingBuffer.getHandle(), width, height);
    textureImage->transitionLayout(vk::ImageLayout::eTransferDstOptimal,
                                    vk::ImageLayout::eShaderReadOnlyOptimal);

    return textureImage;
}

} // namespace MoldWing
