/*
 *  MoldWing - Mesh Loader Implementation
 *  S1.4: Load 3D models using assimp
 *  T6.1.4: Extended for material and texture loading
 */

#include "MeshLoader.hpp"
#include "Core/Logger.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <QFileInfo>
#include <QDir>
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
        MW_LOG_ERROR("assimp error: {}", m_lastError.toStdString());
        return nullptr;
    }

    auto meshData = std::make_shared<MeshData>();

    // Get base directory for texture paths
    QFileInfo fileInfo(filePath);
    QString baseDir = fileInfo.absolutePath();

    // Load materials first (T6.1.4)
    loadMaterials(scene, baseDir, *meshData);

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

        // Copy indices and assign material IDs
        uint32_t materialId = aiM->mMaterialIndex;
        for (unsigned int i = 0; i < aiM->mNumFaces; ++i)
        {
            const aiFace& face = aiM->mFaces[i];
            if (face.mNumIndices == 3)  // Only triangles
            {
                meshData->indices.push_back(baseVertex + face.mIndices[0]);
                meshData->indices.push_back(baseVertex + face.mIndices[1]);
                meshData->indices.push_back(baseVertex + face.mIndices[2]);

                // Store material ID for this face
                meshData->faceMaterialIds.push_back(materialId);
            }
        }
    }

    // Compute bounding box
    meshData->computeBounds();

    // Build face adjacency for selection operations
    meshData->buildAdjacency();

    // Compute face normals for angle-based selection (M5)
    meshData->computeFaceNormals();

    MW_LOG_INFO("Loaded mesh: {} vertices, {} faces, {} materials, {} textures",
                meshData->vertexCount(), meshData->faceCount(),
                meshData->materials.size(), meshData->textures.size());

    return meshData;
}

void MeshLoader::loadMaterials(const aiScene* scene, const QString& baseDir, MeshData& meshData)
{
    if (!scene->HasMaterials())
    {
        // Create a default material
        Material defaultMat("default");
        meshData.materials.push_back(defaultMat);
        return;
    }

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* aiMat = scene->mMaterials[i];

        Material mat;

        // Get material name
        aiString name;
        if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
        {
            mat.name = QString::fromUtf8(name.C_Str());
        }
        else
        {
            mat.name = QString("Material_%1").arg(i);
        }

        // Get diffuse color
        aiColor3D diffuse(0.7f, 0.7f, 0.7f);
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
        {
            mat.diffuseColor[0] = diffuse.r;
            mat.diffuseColor[1] = diffuse.g;
            mat.diffuseColor[2] = diffuse.b;
        }

        // Get ambient color
        aiColor3D ambient(0.2f, 0.2f, 0.2f);
        if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS)
        {
            mat.ambientColor[0] = ambient.r;
            mat.ambientColor[1] = ambient.g;
            mat.ambientColor[2] = ambient.b;
        }

        // Get specular color
        aiColor3D specular(1.0f, 1.0f, 1.0f);
        if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS)
        {
            mat.specularColor[0] = specular.r;
            mat.specularColor[1] = specular.g;
            mat.specularColor[2] = specular.b;
        }

        // Get shininess
        float shininess = 32.0f;
        if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
        {
            mat.shininess = shininess;
        }

        // Get diffuse texture
        if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString texPath;
            if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                QString texPathStr = QString::fromUtf8(texPath.C_Str());

                // Handle relative paths
                QString fullTexPath;
                if (QFileInfo(texPathStr).isRelative())
                {
                    fullTexPath = QDir(baseDir).filePath(texPathStr);
                }
                else
                {
                    fullTexPath = texPathStr;
                }

                mat.diffuseTexPath = fullTexPath;

                // Try to load the texture
                int texId = loadTexture(fullTexPath, meshData);
                mat.textureId = texId;

                if (texId >= 0)
                {
                    MW_LOG_INFO("Loaded texture: {} ({}x{})",
                               fullTexPath.toStdString(),
                               meshData.textures[texId]->width(),
                               meshData.textures[texId]->height());
                }
                else
                {
                    MW_LOG_WARN("Failed to load texture: {}", fullTexPath.toStdString());
                }
            }
        }

        meshData.materials.push_back(mat);
    }
}

int MeshLoader::loadTexture(const QString& texPath, MeshData& meshData)
{
    // Check if texture is already loaded
    for (size_t i = 0; i < meshData.textures.size(); ++i)
    {
        if (meshData.textures[i]->filePath == texPath)
        {
            return static_cast<int>(i);
        }
    }

    // Load new texture
    auto texture = std::make_shared<TextureData>();
    if (texture->load(texPath))
    {
        int texId = static_cast<int>(meshData.textures.size());
        meshData.textures.push_back(texture);
        return texId;
    }

    return -1;
}

} // namespace MoldWing
