/*
 *  MoldWing - Mesh Loader
 *  S1.4: Load 3D models using assimp
 *  T6.1.4: Extended for material and texture loading
 */

#pragma once

#include "Core/MeshData.hpp"
#include <QString>
#include <memory>

// Forward declarations
struct aiScene;

namespace MoldWing
{

class MeshLoader
{
public:
    MeshLoader() = default;

    // Load mesh from file
    std::shared_ptr<MeshData> load(const QString& filePath);

    // Load OBJ specifically
    std::shared_ptr<MeshData> loadOBJ(const QString& filePath);

    // Get last error message
    QString lastError() const { return m_lastError; }

private:
    // Load materials from assimp scene (T6.1.4)
    void loadMaterials(const aiScene* scene, const QString& baseDir, MeshData& meshData);

    // Load texture and return its index, or -1 if failed
    int loadTexture(const QString& texPath, MeshData& meshData);

    QString m_lastError;
};

} // namespace MoldWing
