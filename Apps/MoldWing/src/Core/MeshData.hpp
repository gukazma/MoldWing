/*
 *  MoldWing - Mesh Data Structures
 *  S1.3: Core data structures for 3D mesh
 */

#pragma once

#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "Material.hpp"
#include "TextureData.hpp"

namespace MoldWing
{

// Vertex structure for rendering
struct Vertex
{
    float position[3];  // x, y, z
    float normal[3];    // nx, ny, nz
    float texcoord[2];  // u, v

    Vertex() = default;
    Vertex(float px, float py, float pz,
           float nx, float ny, float nz,
           float u, float v)
        : position{px, py, pz}
        , normal{nx, ny, nz}
        , texcoord{u, v}
    {}
};

// Bounding box
struct BoundingBox
{
    float min[3] = {0, 0, 0};
    float max[3] = {0, 0, 0};

    void expand(float x, float y, float z)
    {
        if (x < min[0]) min[0] = x;
        if (y < min[1]) min[1] = y;
        if (z < min[2]) min[2] = z;
        if (x > max[0]) max[0] = x;
        if (y > max[1]) max[1] = y;
        if (z > max[2]) max[2] = z;
    }

    void reset()
    {
        min[0] = min[1] = min[2] = std::numeric_limits<float>::max();
        max[0] = max[1] = max[2] = std::numeric_limits<float>::lowest();
    }

    float centerX() const { return (min[0] + max[0]) * 0.5f; }
    float centerY() const { return (min[1] + max[1]) * 0.5f; }
    float centerZ() const { return (min[2] + max[2]) * 0.5f; }
    float diagonal() const
    {
        float dx = max[0] - min[0];
        float dy = max[1] - min[1];
        float dz = max[2] - min[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// Face (triangle) data
struct Face
{
    uint32_t indices[3];  // Vertex indices
    uint32_t materialId = 0;
};

// Mesh data structure
struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;  // Triangle indices (3 per face)

    // Material and texture data (M6)
    std::vector<Material> materials;                         // Material list
    std::vector<std::shared_ptr<TextureData>> textures;      // Loaded textures
    std::vector<uint32_t> faceMaterialIds;                   // Material ID per face

    // Computed data
    BoundingBox bounds;

    // Face adjacency (for selection operations)
    // faceAdjacency[faceId] = set of adjacent face IDs
    std::vector<std::unordered_set<uint32_t>> faceAdjacency;

    // Face normals (computed for angle-based selection)
    // faceNormals[faceId] = {nx, ny, nz}
    std::vector<std::array<float, 3>> faceNormals;

    // Helper methods
    uint32_t faceCount() const { return static_cast<uint32_t>(indices.size() / 3); }
    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices.size()); }

    void computeBounds()
    {
        bounds.reset();
        for (const auto& v : vertices)
        {
            bounds.expand(v.position[0], v.position[1], v.position[2]);
        }
    }

    void buildAdjacency();
    void computeFaceNormals();

    void clear()
    {
        vertices.clear();
        indices.clear();
        materials.clear();
        textures.clear();
        faceMaterialIds.clear();
        faceAdjacency.clear();
        faceNormals.clear();
    }

    // Check if mesh has textures
    bool hasTextures() const { return !textures.empty(); }

    // Get material for a face
    const Material* getFaceMaterial(uint32_t faceId) const
    {
        if (faceId < faceMaterialIds.size())
        {
            uint32_t matId = faceMaterialIds[faceId];
            if (matId < materials.size())
            {
                return &materials[matId];
            }
        }
        return nullptr;
    }
};

} // namespace MoldWing
