/*
 *  MoldWing - Mesh Loader
 *  S1.4: Load 3D models using assimp
 */

#pragma once

#include "Core/MeshData.hpp"
#include <QString>
#include <memory>

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
    QString m_lastError;
};

} // namespace MoldWing
