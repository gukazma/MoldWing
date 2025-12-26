/*
 *  MoldWing - Texture Data Structure
 *  S1.3: Texture data using QImage
 */

#pragma once

#include <QImage>
#include <QString>
#include <memory>

namespace MoldWing
{

// Texture data structure
struct TextureData
{
    QImage image;           // Texture image data
    QString filePath;       // Source file path
    bool modified = false;  // Has been edited

    // GPU texture handle (set by renderer)
    void* gpuTexture = nullptr;

    TextureData() = default;

    explicit TextureData(const QString& path)
        : filePath(path)
    {
        load(path);
    }

    bool load(const QString& path)
    {
        if (image.load(path))
        {
            filePath = path;
            // Convert to RGBA format for GPU
            image = image.convertToFormat(QImage::Format_RGBA8888);
            modified = false;
            return true;
        }
        return false;
    }

    bool save(const QString& path)
    {
        if (image.save(path))
        {
            filePath = path;
            modified = false;
            return true;
        }
        return false;
    }

    int width() const { return image.width(); }
    int height() const { return image.height(); }
    bool isValid() const { return !image.isNull(); }

    const uchar* data() const { return image.constBits(); }
    int bytesPerLine() const { return image.bytesPerLine(); }
};

} // namespace MoldWing
