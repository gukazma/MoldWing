#include "MoldWing/Mesh/Mesh.h"

#include <CGAL/IO/OBJ.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <tiny_obj_loader.h>
#include <unordered_map>

namespace MoldWing {

bool Mesh::loadFromOBJ(const std::string& filepath) {
    // Use tinyobjloader to parse OBJ file (including UV coordinates)
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        std::cerr << "Failed to load OBJ file: " << filepath << std::endl;
        if (!warn.empty()) {
            std::cerr << "Warning: " << warn << std::endl;
        }
        if (!err.empty()) {
            std::cerr << "Error: " << err << std::endl;
        }
        return false;
    }

    if (!warn.empty()) {
        std::cout << "Warning: " << warn << std::endl;
    }

    // Build CGAL mesh and store UV coordinates
    // Map from (pos_index) to CGAL vertex descriptor
    std::unordered_map<int, CGALMesh::Vertex_index> vertex_map;

    // Track vertex index for UV mapping
    std::vector<glm::vec2> vertex_uvs;

    // Add vertices to CGAL mesh
    for (size_t i = 0; i < attrib.vertices.size() / 3; ++i) {
        Point3 p(attrib.vertices[3 * i + 0],
                 attrib.vertices[3 * i + 1],
                 attrib.vertices[3 * i + 2]);
        auto v = m_cgalMesh.add_vertex(p);
        vertex_map[static_cast<int>(i)] = v;
    }

    // Initialize vertex attributes with default UV
    m_vertexAttributes.resize(attrib.vertices.size() / 3);
    for (auto& attr : m_vertexAttributes) {
        attr.texCoord = glm::vec2(0.0f, 0.0f);
    }

    // Add faces and extract UV coordinates
    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];

            std::vector<CGALMesh::Vertex_index> face_vertices;

            for (int v = 0; v < fv; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                // Get CGAL vertex
                int pos_idx = idx.vertex_index;
                auto cgal_v = vertex_map[pos_idx];
                face_vertices.push_back(cgal_v);

                // Store UV coordinate if available
                if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                    glm::vec2 uv(attrib.texcoords[2 * idx.texcoord_index + 0],
                                 attrib.texcoords[2 * idx.texcoord_index + 1]);
                    m_vertexAttributes[pos_idx].texCoord = uv;
                }
            }

            // Add face to CGAL mesh (only if valid triangle)
            if (face_vertices.size() == 3) {
                m_cgalMesh.add_face(face_vertices[0], face_vertices[1], face_vertices[2]);
            } else if (face_vertices.size() > 3) {
                // Triangulate polygon (simple fan triangulation)
                for (size_t i = 1; i + 1 < face_vertices.size(); ++i) {
                    m_cgalMesh.add_face(face_vertices[0], face_vertices[i], face_vertices[i + 1]);
                }
            }

            index_offset += fv;
        }
    }

    std::cout << "Loaded mesh from: " << filepath << std::endl;
    std::cout << "  Vertices: " << m_cgalMesh.num_vertices() << std::endl;
    std::cout << "  Faces: " << m_cgalMesh.num_faces() << std::endl;
    std::cout << "  UV coords: " << (attrib.texcoords.size() / 2) << std::endl;

    // Compute normals
    computeVertexNormals();

    return true;
}

bool Mesh::saveToOBJ(const std::string& filepath) const {
    std::ofstream output(filepath);
    if (!output) {
        std::cerr << "Failed to create file: " << filepath << std::endl;
        return false;
    }

    if (!CGAL::IO::write_OBJ(output, m_cgalMesh)) {
        std::cerr << "Failed to write OBJ file: " << filepath << std::endl;
        return false;
    }

    std::cout << "Saved mesh to: " << filepath << std::endl;
    return true;
}

void Mesh::setVertexAttribute(size_t index, const VertexAttributes& attr) {
    if (index < m_vertexAttributes.size()) {
        m_vertexAttributes[index] = attr;
    }
}

void Mesh::computeVertexNormals() {
    // Simple vertex normal computation (average of adjacent face normals)
    m_vertexAttributes.resize(m_cgalMesh.num_vertices());

    // Initialize all normals to zero
    for (auto& attr : m_vertexAttributes) {
        attr.normal = glm::vec3(0.0f);
    }

    // Accumulate face normals
    for (auto face : m_cgalMesh.faces()) {
        // Get vertices of the face
        auto vertices = m_cgalMesh.vertices_around_face(m_cgalMesh.halfedge(face));
        std::vector<Point3> points;
        for (auto v : vertices) {
            points.push_back(m_cgalMesh.point(v));
        }

        if (points.size() >= 3) {
            // Compute face normal
            Vector3 v1(points[0], points[1]);
            Vector3 v2(points[0], points[2]);
            Vector3 normal = CGAL::cross_product(v1, v2);

            // Normalize
            double len = std::sqrt(normal.squared_length());
            if (len > 1e-10) {
                normal = normal / len;
            }

            // Add to vertex normals
            for (auto v : m_cgalMesh.vertices_around_face(m_cgalMesh.halfedge(face))) {
                size_t vidx = v.idx();
                m_vertexAttributes[vidx].normal +=
                    glm::vec3(static_cast<float>(normal.x()), static_cast<float>(normal.y()),
                              static_cast<float>(normal.z()));
            }
        }
    }

    // Normalize vertex normals
    for (auto& attr : m_vertexAttributes) {
        float len = glm::length(attr.normal);
        if (len > 1e-6f) {
            attr.normal /= len;
        }
    }
}

void Mesh::computeFaceNormals() {
    // Face normals are computed on-the-fly in computeVertexNormals
    // This is a placeholder for future face-based normal storage
}

void Mesh::getBoundingBox(glm::vec3& min, glm::vec3& max) const {
    if (m_cgalMesh.num_vertices() == 0) {
        min = max = glm::vec3(0.0f);
        return;
    }

    auto first_vertex = m_cgalMesh.vertices().begin();
    Point3 first_point = m_cgalMesh.point(*first_vertex);

    min = max =
        glm::vec3(static_cast<float>(first_point.x()), static_cast<float>(first_point.y()),
                  static_cast<float>(first_point.z()));

    for (auto v : m_cgalMesh.vertices()) {
        Point3 p = m_cgalMesh.point(v);
        glm::vec3 pos(static_cast<float>(p.x()), static_cast<float>(p.y()),
                      static_cast<float>(p.z()));

        min = glm::min(min, pos);
        max = glm::max(max, pos);
    }
}

void Mesh::centerAtOrigin() {
    glm::vec3 min, max;
    getBoundingBox(min, max);

    glm::vec3 center = (min + max) * 0.5f;

    for (auto v : m_cgalMesh.vertices()) {
        Point3 p = m_cgalMesh.point(v);
        Point3 new_p(p.x() - center.x, p.y() - center.y, p.z() - center.z);
        m_cgalMesh.point(v) = new_p;
    }
}

void Mesh::normalizeScale() {
    glm::vec3 min, max;
    getBoundingBox(min, max);

    glm::vec3 size = max - min;
    float maxDim = std::max({size.x, size.y, size.z});

    if (maxDim > 1e-6f) {
        float scale = 2.0f / maxDim;

        for (auto v : m_cgalMesh.vertices()) {
            Point3 p = m_cgalMesh.point(v);
            Point3 new_p(p.x() * scale, p.y() * scale, p.z() * scale);
            m_cgalMesh.point(v) = new_p;
        }
    }
}

void Mesh::toRenderData(std::vector<float>& vertices, std::vector<uint32_t>& indices) const {
    vertices.clear();
    indices.clear();

    // Reserve space
    vertices.reserve(m_cgalMesh.num_vertices() * 8); // pos(3) + normal(3) + texCoord(2)
    indices.reserve(m_cgalMesh.num_faces() * 3);

    // Export vertices
    size_t idx = 0;
    for (auto v : m_cgalMesh.vertices()) {
        Point3 p = m_cgalMesh.point(v);

        // Position
        vertices.push_back(static_cast<float>(p.x()));
        vertices.push_back(static_cast<float>(p.y()));
        vertices.push_back(static_cast<float>(p.z()));

        // Normal
        if (idx < m_vertexAttributes.size()) {
            vertices.push_back(m_vertexAttributes[idx].normal.x);
            vertices.push_back(m_vertexAttributes[idx].normal.y);
            vertices.push_back(m_vertexAttributes[idx].normal.z);

            // TexCoord
            vertices.push_back(m_vertexAttributes[idx].texCoord.x);
            vertices.push_back(m_vertexAttributes[idx].texCoord.y);
        } else {
            // Default normal and texcoord
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
        }

        idx++;
    }

    // Export indices (triangulate faces)
    for (auto face : m_cgalMesh.faces()) {
        auto vertices_range = m_cgalMesh.vertices_around_face(m_cgalMesh.halfedge(face));
        std::vector<uint32_t> face_indices;

        for (auto v : vertices_range) {
            face_indices.push_back(static_cast<uint32_t>(v.idx()));
        }

        // Triangulate (simple fan triangulation)
        if (face_indices.size() >= 3) {
            for (size_t i = 1; i + 1 < face_indices.size(); ++i) {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[i]);
                indices.push_back(face_indices[i + 1]);
            }
        }
    }
}

} // namespace MoldWing
