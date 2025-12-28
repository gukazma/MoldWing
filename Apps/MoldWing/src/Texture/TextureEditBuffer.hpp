/*
 *  MoldWing - Texture Edit Buffer
 *  S6.3: CPU texture copy for editing with dirty region tracking
 */

#pragma once

#include "Core/TextureData.hpp"

#include <QRect>
#include <QString>
#include <vector>
#include <memory>
#include <cstdint>

namespace MoldWing
{

/**
 * TextureEditBuffer holds a CPU copy of a texture for editing.
 * It tracks "dirty" regions that have been modified and need GPU sync.
 */
class TextureEditBuffer
{
public:
    TextureEditBuffer() = default;
    ~TextureEditBuffer() = default;

    // Initialize from a TextureData (creates a copy)
    bool initialize(const TextureData& source);

    // Get buffer dimensions
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isValid() const { return !m_data.empty() && m_width > 0 && m_height > 0; }

    // Pixel access (RGBA format)
    uint8_t* pixelAt(int x, int y);
    const uint8_t* pixelAt(int x, int y) const;

    // Get pixel color
    void getPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const;

    // Set pixel color and mark dirty
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Bulk pixel access
    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }
    size_t bytesPerLine() const { return m_width * 4; }

    // Dirty region tracking
    bool isDirty() const { return !m_dirtyRects.empty(); }
    const std::vector<QRect>& dirtyRects() const { return m_dirtyRects; }
    void markDirty(const QRect& rect);
    void markDirty(int x, int y, int width, int height);
    void clearDirty();

    // Get union of all dirty rects (for efficient GPU update)
    QRect dirtyBounds() const;

    // Original texture backup (for undo/eraser)
    bool hasOriginal() const { return !m_originalData.empty(); }
    const uint8_t* originalPixelAt(int x, int y) const;
    void getOriginalPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const;

    // Copy current state to a TextureData for GPU upload
    std::shared_ptr<TextureData> toTextureData() const;

    // Save to file
    bool save(const QString& filePath) const;

    // Check if modified from original
    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_data;        // Current editable data (RGBA)
    std::vector<uint8_t> m_originalData; // Original backup for undo/eraser
    std::vector<QRect> m_dirtyRects;    // Regions that need GPU sync
    bool m_modified = false;
};

} // namespace MoldWing
