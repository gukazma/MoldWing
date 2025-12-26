/*
 *  MoldWing - Mesh Loader Implementation
 *  S1.4: Load 3D models using assimp
 */

#include "MeshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <QFileInfo>
#include <QDebug>

namespace MoldWing
{

std::shared_ptr<MeshData> MeshLoader::load(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (extension == "obj")
    {
        return loadOBJ(filePath);
    }
    else
    {
        // Use assimp for other formats
        return loadOBJ(filePath);  // assimp handles multiple formats
    }
}

std::shared_ptr<MeshData> MeshLoader::loadOBJ(const QString& filePath)
{
    Assimp::Importer importer;

    // Import flags
    unsigned int flags = aiProcess_Triangulate |           // Ensure triangles
                         aiProcess_GenSmoothNormals |      // Generate normals if missing
                         aiProcess_FlipUVs |               // Flip UV for OpenGL/DX style
                         aiProcess_JoinIdenticalVertices | // Optimize vertices
                         aiProcess_CalcTangentSpace;       // Calculate tangents

    const aiScene* scene = importer.ReadFile(
        filePath.toStdString(), flags);

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        m_lastError = QString::fromStdString(importer.GetErrorString());
        qWarning() << "assimp error:" << m_lastError;
        return nullptr;
    }

    auto meshData = std::make_shared<MeshData>();

    // Process all meshes in the scene
    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
    {
        const aiMesh* aiM = scene->mMeshes[meshIdx];

        uint32_t baseVertex = static_cast<uint32_t>(meshData->vertices.size());

        // Copy vertices
        for (unsigned int i = 0; i < aiM->mNumVertices; ++i)
        {
            Vertex v;

            // Position
            v.position[0] = aiM->mVertices[i].x;
            v.position[1] = aiM->mVertices[i].y;
            v.position[2] = aiM->mVertices[i].z;

            // Normal
            if (aiM->HasNormals())
            {
                v.normal[0] = aiM->mNormals[i].x;
                v.normal[1] = aiM->mNormals[i].y;
                v.normal[2] = aiM->mNormals[i].z;
            }
            else
            {
                v.normal[0] = 0.0f;
                v.normal[1] = 1.0f;
                v.normal[2] = 0.0f;
            }

            // Texture coordinates
            if (aiM->HasTextureCoords(0))
            {
                v.texcoord[0] = aiM->mTextureCoords[0][i].x;
                v.texcoord[1] = aiM->mTextureCoords[0][i].y;
            }
            else
            {
                v.texcoord[0] = 0.0f;
                v.texcoord[1] = 0.0f;
            }

            meshData->vertices.push_back(v);
        }

        // Copy indices
        for (unsigned int i = 0; i < aiM->mNumFaces; ++i)
        {
            const aiFace& face = aiM->mFaces[i];
            if (face.mNumIndices == 3)  // Only triangles
            {
                meshData->indices.push_back(baseVertex + face.mIndices[0]);
                meshData->indices.push_back(baseVertex + face.mIndices[1]);
                meshData->indices.push_back(baseVertex + face.mIndices[2]);
            }
        }
    }

    // Compute bounding box
    meshData->computeBounds();

    // Build face adjacency for selection operations
    meshData->buildAdjacency();

    qDebug() << "Loaded mesh:" << filePath
             << "| Vertices:" << meshData->vertexCount()
             << "| Faces:" << meshData->faceCount();

    return meshData;
}

} // namespace MoldWing
