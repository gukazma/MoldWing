/*
 *  MoldWing - Material Data Structure
 *  T6.1.1: Material definition with texture support
 */

#pragma once

#include <QString>
#include <vector>
#include <cstdint>

namespace MoldWing
{

// Material data structure
struct Material
{
    QString name;                  // Material name (from MTL file)
    QString diffuseTexPath;        // Path to diffuse texture
    int32_t textureId = -1;        // Index into MeshData::textures (-1 = no texture)

    // Material colors (for non-textured rendering)
    float diffuseColor[3] = {0.7f, 0.7f, 0.7f};   // Kd
    float ambientColor[3] = {0.2f, 0.2f, 0.2f};   // Ka
    float specularColor[3] = {1.0f, 1.0f, 1.0f};  // Ks
    float shininess = 32.0f;                       // Ns

    Material() = default;

    explicit Material(const QString& materialName)
        : name(materialName)
    {
    }

    bool hasTexture() const { return textureId >= 0; }
};

} // namespace MoldWing
