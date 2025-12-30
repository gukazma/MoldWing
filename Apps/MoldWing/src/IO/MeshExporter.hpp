/*
 *  MoldWing - Mesh Exporter
 *  M7.5: Export models with edited textures to OBJ format
 */

#pragma once

#include "Core/MeshData.hpp"
#include "Texture/TextureEditBuffer.hpp"
#include <QString>
#include <memory>
#include <unordered_map>

namespace MoldWing
{

class MeshExporter
{
public:
    MeshExporter() = default;

    // Export mesh to OBJ file with MTL and textures
    // If editBuffers is provided, use edited texture data instead of original
    bool exportOBJ(
        const QString& filePath,
        const MeshData& meshData,
        const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers = {}
    );

    // Get last error message
    QString lastError() const { return m_lastError; }

private:
    // Write OBJ file
    bool writeOBJ(const QString& filePath, const MeshData& meshData, const QString& mtlFileName);

    // Write MTL file
    bool writeMTL(
        const QString& filePath,
        const MeshData& meshData,
        const QString& baseDir,
        const std::unordered_map<int, QString>& textureFileNames
    );

    // Export textures and return mapping of textureId -> exported filename
    std::unordered_map<int, QString> exportTextures(
        const QString& baseDir,
        const QString& baseName,
        const MeshData& meshData,
        const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers
    );

    // Get file extension from path
    QString getFileExtension(const QString& path) const;

    QString m_lastError;
};

} // namespace MoldWing
