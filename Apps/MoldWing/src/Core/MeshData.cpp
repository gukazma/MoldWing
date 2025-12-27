/*
 *  MoldWing - Mesh Data Implementation
 *  S1.3: Core data structures
 */

#include "MeshData.hpp"
#include <map>
#include <cmath>

namespace MoldWing
{

void MeshData::buildAdjacency()
{
    uint32_t numFaces = faceCount();
    faceAdjacency.clear();
    faceAdjacency.resize(numFaces);

    // Build edge to face map
    // Edge key: (min_vertex_id, max_vertex_id)
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edgeToFaces;

    for (uint32_t faceId = 0; faceId < numFaces; ++faceId)
    {
        uint32_t baseIdx = faceId * 3;
        uint32_t v0 = indices[baseIdx + 0];
        uint32_t v1 = indices[baseIdx + 1];
        uint32_t v2 = indices[baseIdx + 2];

        // Add all three edges
        auto addEdge = [&](uint32_t a, uint32_t b) {
            auto key = std::make_pair(std::min(a, b), std::max(a, b));
            edgeToFaces[key].push_back(faceId);
        };

        addEdge(v0, v1);
        addEdge(v1, v2);
        addEdge(v2, v0);
    }

    // Build adjacency from shared edges
    for (const auto& [edge, faces] : edgeToFaces)
    {
        // Each face sharing this edge is adjacent to each other
        for (size_t i = 0; i < faces.size(); ++i)
        {
            for (size_t j = i + 1; j < faces.size(); ++j)
            {
                faceAdjacency[faces[i]].insert(faces[j]);
                faceAdjacency[faces[j]].insert(faces[i]);
            }
        }
    }
}

void MeshData::computeFaceNormals()
{
    uint32_t numFaces = faceCount();
    faceNormals.clear();
    faceNormals.resize(numFaces);

    for (uint32_t faceId = 0; faceId < numFaces; ++faceId)
    {
        uint32_t baseIdx = faceId * 3;
        uint32_t i0 = indices[baseIdx + 0];
        uint32_t i1 = indices[baseIdx + 1];
        uint32_t i2 = indices[baseIdx + 2];

        // Get vertex positions
        const float* p0 = vertices[i0].position;
        const float* p1 = vertices[i1].position;
        const float* p2 = vertices[i2].position;

        // Compute edge vectors
        float e1x = p1[0] - p0[0];
        float e1y = p1[1] - p0[1];
        float e1z = p1[2] - p0[2];

        float e2x = p2[0] - p0[0];
        float e2y = p2[1] - p0[1];
        float e2z = p2[2] - p0[2];

        // Cross product: e1 x e2
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;

        // Normalize
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-8f)
        {
            nx /= len;
            ny /= len;
            nz /= len;
        }
        else
        {
            // Degenerate triangle, use zero normal
            nx = ny = nz = 0.0f;
        }

        faceNormals[faceId] = {nx, ny, nz};
    }
}

} // namespace MoldWing
