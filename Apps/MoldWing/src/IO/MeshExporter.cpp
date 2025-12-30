/*
 *  MoldWing - Mesh Exporter Implementation
 *  M7.5: Export models with edited textures to OBJ format
 */

#include "MeshExporter.hpp"
#include "Core/Logger.hpp"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QImage>

namespace MoldWing
{

bool MeshExporter::exportOBJ(
    const QString& filePath,
    const MeshData& meshData,
    const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers)
{
    if (meshData.vertices.empty())
    {
        m_lastError = "No mesh data to export";
        return false;
    }

    QFileInfo fileInfo(filePath);
    QString baseDir = fileInfo.absolutePath();
    QString baseName = fileInfo.completeBaseName();
    QString mtlFileName = baseName + ".mtl";
    QString mtlFilePath = QDir(baseDir).filePath(mtlFileName);

    // Export textures first and get the mapping
    auto textureFileNames = exportTextures(baseDir, baseName, meshData, editBuffers);

    // Write MTL file
    if (!meshData.materials.empty())
    {
        if (!writeMTL(mtlFilePath, meshData, baseDir, textureFileNames))
        {
            return false;
        }
    }

    // Write OBJ file
    if (!writeOBJ(filePath, meshData, mtlFileName))
    {
        return false;
    }

    MW_LOG_INFO("Exported OBJ: {} ({} vertices, {} faces)",
                filePath.toStdString(),
                meshData.vertexCount(),
                meshData.faceCount());

    return true;
}

bool MeshExporter::writeOBJ(const QString& filePath, const MeshData& meshData, const QString& mtlFileName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_lastError = QString("Cannot open file for writing: %1").arg(filePath);
        MW_LOG_ERROR("{}", m_lastError.toStdString());
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(6);

    // Header
    out << "# MoldWing Export\n";
    out << "# Vertices: " << meshData.vertexCount() << "\n";
    out << "# Faces: " << meshData.faceCount() << "\n";
    out << "\n";

    // Material library reference
    if (!meshData.materials.empty())
    {
        out << "mtllib " << mtlFileName << "\n";
        out << "\n";
    }

    // Write vertices (v x y z)
    for (const auto& v : meshData.vertices)
    {
        out << "v " << v.position[0] << " " << v.position[1] << " " << v.position[2] << "\n";
    }
    out << "\n";

    // Write texture coordinates (vt u v)
    // Note: Flip V back because assimp flipped UV on load (aiProcess_FlipUVs)
    for (const auto& v : meshData.vertices)
    {
        out << "vt " << v.texcoord[0] << " " << (1.0f - v.texcoord[1]) << "\n";
    }
    out << "\n";

    // Write normals (vn nx ny nz)
    for (const auto& v : meshData.vertices)
    {
        out << "vn " << v.normal[0] << " " << v.normal[1] << " " << v.normal[2] << "\n";
    }
    out << "\n";

    // Write faces grouped by material
    // OBJ uses 1-based indices
    uint32_t currentMaterialId = UINT32_MAX;
    uint32_t faceCount = meshData.faceCount();

    for (uint32_t faceIdx = 0; faceIdx < faceCount; ++faceIdx)
    {
        // Check if we need to switch material
        uint32_t materialId = 0;
        if (faceIdx < meshData.faceMaterialIds.size())
        {
            materialId = meshData.faceMaterialIds[faceIdx];
        }

        if (materialId != currentMaterialId)
        {
            currentMaterialId = materialId;
            if (materialId < meshData.materials.size())
            {
                out << "\nusemtl " << meshData.materials[materialId].name << "\n";
            }
        }

        // Get vertex indices for this face
        uint32_t idx0 = meshData.indices[faceIdx * 3 + 0];
        uint32_t idx1 = meshData.indices[faceIdx * 3 + 1];
        uint32_t idx2 = meshData.indices[faceIdx * 3 + 2];

        // Skip degenerate triangles (faces with duplicate vertex indices)
        if (idx0 == idx1 || idx1 == idx2 || idx0 == idx2)
        {
            continue;
        }

        // OBJ uses 1-based indices, format: f v/vt/vn v/vt/vn v/vt/vn
        out << "f "
            << (idx0 + 1) << "/" << (idx0 + 1) << "/" << (idx0 + 1) << " "
            << (idx1 + 1) << "/" << (idx1 + 1) << "/" << (idx1 + 1) << " "
            << (idx2 + 1) << "/" << (idx2 + 1) << "/" << (idx2 + 1) << "\n";
    }

    file.close();
    return true;
}

bool MeshExporter::writeMTL(
    const QString& filePath,
    const MeshData& meshData,
    const QString& baseDir,
    const std::unordered_map<int, QString>& textureFileNames)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_lastError = QString("Cannot open MTL file for writing: %1").arg(filePath);
        MW_LOG_ERROR("{}", m_lastError.toStdString());
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(4);

    // Header
    out << "# MoldWing Material Library\n";
    out << "\n";

    for (const auto& mat : meshData.materials)
    {
        out << "newmtl " << mat.name << "\n";

        // Ambient color
        out << "Ka " << mat.ambientColor[0] << " " << mat.ambientColor[1] << " " << mat.ambientColor[2] << "\n";

        // Diffuse color
        out << "Kd " << mat.diffuseColor[0] << " " << mat.diffuseColor[1] << " " << mat.diffuseColor[2] << "\n";

        // Specular color
        out << "Ks " << mat.specularColor[0] << " " << mat.specularColor[1] << " " << mat.specularColor[2] << "\n";

        // Shininess (Ns)
        out << "Ns " << mat.shininess << "\n";

        // Illumination model (2 = highlight on)
        out << "illum 2\n";

        // Diffuse texture map
        if (mat.textureId >= 0)
        {
            auto it = textureFileNames.find(mat.textureId);
            if (it != textureFileNames.end())
            {
                out << "map_Kd " << it->second << "\n";
            }
        }

        out << "\n";
    }

    file.close();

    MW_LOG_INFO("Exported MTL: {} ({} materials)",
                filePath.toStdString(),
                meshData.materials.size());

    return true;
}

std::unordered_map<int, QString> MeshExporter::exportTextures(
    const QString& baseDir,
    const QString& baseName,
    const MeshData& meshData,
    const std::unordered_map<int, std::shared_ptr<TextureEditBuffer>>& editBuffers)
{
    std::unordered_map<int, QString> result;

    for (size_t i = 0; i < meshData.textures.size(); ++i)
    {
        const auto& texture = meshData.textures[i];
        if (!texture || !texture->isValid())
        {
            continue;
        }

        int texId = static_cast<int>(i);

        // Determine output filename and format
        QString originalExt = getFileExtension(texture->filePath);
        if (originalExt.isEmpty())
        {
            originalExt = "png";  // Default to PNG
        }

        QString texFileName;
        if (meshData.textures.size() == 1)
        {
            texFileName = baseName + "_texture." + originalExt;
        }
        else
        {
            texFileName = QString("%1_texture_%2.%3").arg(baseName).arg(i).arg(originalExt);
        }

        QString texFilePath = QDir(baseDir).filePath(texFileName);

        // Check if we have an edited version
        auto editIt = editBuffers.find(texId);
        bool saved = false;

        if (editIt != editBuffers.end() && editIt->second && editIt->second->isValid())
        {
            // Save edited texture from buffer
            saved = editIt->second->save(texFilePath);
            if (saved)
            {
                MW_LOG_INFO("Exported edited texture: {}", texFilePath.toStdString());
            }
        }
        else
        {
            // Save original texture
            // Try to copy the original file if it exists and format matches
            QFileInfo originalFile(texture->filePath);
            if (originalFile.exists() && getFileExtension(texture->filePath).toLower() == originalExt.toLower())
            {
                // Copy original file
                if (QFile::exists(texFilePath))
                {
                    QFile::remove(texFilePath);
                }
                saved = QFile::copy(texture->filePath, texFilePath);
                if (saved)
                {
                    MW_LOG_INFO("Copied original texture: {}", texFilePath.toStdString());
                }
            }

            if (!saved)
            {
                // Save from loaded image data
                saved = texture->image.save(texFilePath);
                if (saved)
                {
                    MW_LOG_INFO("Exported texture from memory: {}", texFilePath.toStdString());
                }
            }
        }

        if (saved)
        {
            result[texId] = texFileName;  // Store relative filename for MTL
        }
        else
        {
            MW_LOG_WARN("Failed to export texture: {}", texFilePath.toStdString());
        }
    }

    return result;
}

QString MeshExporter::getFileExtension(const QString& path) const
{
    QFileInfo fileInfo(path);
    return fileInfo.suffix().toLower();
}

} // namespace MoldWing
