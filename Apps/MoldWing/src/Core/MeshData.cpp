/*
 *  MoldWing - Mesh Data Implementation
 *  S1.3: Core data structures
 */

#include "MeshData.hpp"
#include <map>

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

} // namespace MoldWing
