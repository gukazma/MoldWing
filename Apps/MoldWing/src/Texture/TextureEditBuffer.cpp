/*
 *  MoldWing - Texture Edit Buffer Implementation
 *  S6.3: CPU texture copy for editing with dirty region tracking
 */

#include "TextureEditBuffer.hpp"
#include "Core/Logger.hpp"

#include <algorithm>
#include <cstring>

// For saving PNG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace MoldWing
{

bool TextureEditBuffer::initialize(const TextureData& source)
{
    if (!source.isValid())
    {
        MW_LOG_ERROR("Cannot initialize TextureEditBuffer from invalid source");
        return false;
    }

    m_width = source.width();
    m_height = source.height();

    // Allocate buffer
    size_t dataSize = m_width * m_height * 4;
    m_data.resize(dataSize);
    m_originalData.resize(dataSize);

    // Copy data from source
    std::memcpy(m_data.data(), source.data(), dataSize);
    std::memcpy(m_originalData.data(), source.data(), dataSize);

    m_dirtyRects.clear();
    m_modified = false;

    MW_LOG_INFO("TextureEditBuffer initialized: {}x{}", m_width, m_height);
    return true;
}

uint8_t* TextureEditBuffer::pixelAt(int x, int y)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return nullptr;

    return &m_data[(y * m_width + x) * 4];
}

const uint8_t* TextureEditBuffer::pixelAt(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return nullptr;

    return &m_data[(y * m_width + x) * 4];
}

void TextureEditBuffer::getPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const
{
    const uint8_t* p = pixelAt(x, y);
    if (p)
    {
        r = p[0];
        g = p[1];
        b = p[2];
        a = p[3];
    }
    else
    {
        r = g = b = a = 0;
    }
}

void TextureEditBuffer::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uint8_t* p = pixelAt(x, y);
    if (p)
    {
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
        markDirty(x, y, 1, 1);
        m_modified = true;
    }
}

void TextureEditBuffer::markDirty(const QRect& rect)
{
    // Clamp to texture bounds
    QRect clamped = rect.intersected(QRect(0, 0, m_width, m_height));
    if (clamped.isEmpty())
        return;

    // Simple approach: just add the rect
    // A more sophisticated approach would merge overlapping rects
    m_dirtyRects.push_back(clamped);
}

void TextureEditBuffer::markDirty(int x, int y, int width, int height)
{
    markDirty(QRect(x, y, width, height));
}

void TextureEditBuffer::clearDirty()
{
    m_dirtyRects.clear();
}

QRect TextureEditBuffer::dirtyBounds() const
{
    if (m_dirtyRects.empty())
        return QRect();

    QRect bounds = m_dirtyRects[0];
    for (size_t i = 1; i < m_dirtyRects.size(); ++i)
    {
        bounds = bounds.united(m_dirtyRects[i]);
    }
    return bounds;
}

const uint8_t* TextureEditBuffer::originalPixelAt(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || m_originalData.empty())
        return nullptr;

    return &m_originalData[(y * m_width + x) * 4];
}

void TextureEditBuffer::getOriginalPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const
{
    const uint8_t* p = originalPixelAt(x, y);
    if (p)
    {
        r = p[0];
        g = p[1];
        b = p[2];
        a = p[3];
    }
    else
    {
        r = g = b = a = 0;
    }
}

std::shared_ptr<TextureData> TextureEditBuffer::toTextureData() const
{
    if (!isValid())
        return nullptr;

    auto texData = std::make_shared<TextureData>();
    // TextureData stores RGBA, same as our buffer
    // We need to manually set the data since TextureData uses QImage internally
    // For now, create from raw data

    // Note: TextureData's load() expects a file path
    // We'll need to add a method to TextureData to load from raw data
    // For now, return nullptr and implement this later

    MW_LOG_WARN("toTextureData() not fully implemented yet");
    return nullptr;
}

bool TextureEditBuffer::save(const QString& filePath) const
{
    if (!isValid())
    {
        MW_LOG_ERROR("Cannot save invalid TextureEditBuffer");
        return false;
    }

    std::string path = filePath.toStdString();
    QString ext = filePath.section('.', -1).toLower();

    int result = 0;
    if (ext == "png")
    {
        result = stbi_write_png(path.c_str(), m_width, m_height, 4, m_data.data(), m_width * 4);
    }
    else if (ext == "jpg" || ext == "jpeg")
    {
        result = stbi_write_jpg(path.c_str(), m_width, m_height, 4, m_data.data(), 90);
    }
    else if (ext == "bmp")
    {
        result = stbi_write_bmp(path.c_str(), m_width, m_height, 4, m_data.data());
    }
    else if (ext == "tga")
    {
        result = stbi_write_tga(path.c_str(), m_width, m_height, 4, m_data.data());
    }
    else
    {
        // Default to PNG
        result = stbi_write_png(path.c_str(), m_width, m_height, 4, m_data.data(), m_width * 4);
    }

    if (result == 0)
    {
        MW_LOG_ERROR("Failed to save texture to: {}", path);
        return false;
    }

    MW_LOG_INFO("Saved texture to: {} ({}x{})", path, m_width, m_height);
    return true;
}

} // namespace MoldWing
