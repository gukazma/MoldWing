#pragma once

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifdef MESH_EXPORTS
#define MESH_API __declspec(dllexport)
#else
#define MESH_API __declspec(dllimport)
#endif
#else
#define MESH_API
#endif

namespace MoldWing {

// CGAL types
using Kernel = CGAL::Simple_cartesian<double>;
using Point3 = Kernel::Point_3;
using Vector3 = Kernel::Vector_3;
using CGALMesh = CGAL::Surface_mesh<Point3>;

/**
 * @brief Mesh class that wraps CGAL Surface_mesh with additional functionality
 */
class MESH_API Mesh {
  public:
    Mesh() = default;
    ~Mesh() = default;

    // Access to underlying CGAL mesh
    CGALMesh& getCGALMesh() { return m_cgalMesh; }
    const CGALMesh& getCGALMesh() const { return m_cgalMesh; }

    // Vertex attributes
    struct VertexAttributes {
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec3 color;
    };

    /**
     * @brief Load mesh from OBJ file
     * @param filepath Path to OBJ file
     * @return true if successful
     */
    bool loadFromOBJ(const std::string& filepath);

    /**
     * @brief Save mesh to OBJ file
     * @param filepath Path to save OBJ file
     * @return true if successful
     */
    bool saveToOBJ(const std::string& filepath) const;

    /**
     * @brief Get vertex attributes
     */
    const std::vector<VertexAttributes>& getVertexAttributes() const { return m_vertexAttributes; }

    /**
     * @brief Set vertex attribute
     */
    void setVertexAttribute(size_t index, const VertexAttributes& attr);

    /**
     * @brief Compute vertex normals
     */
    void computeVertexNormals();

    /**
     * @brief Compute face normals
     */
    void computeFaceNormals();

    /**
     * @brief Get mesh bounding box
     */
    void getBoundingBox(glm::vec3& min, glm::vec3& max) const;

    /**
     * @brief Center mesh at origin
     */
    void centerAtOrigin();

    /**
     * @brief Scale mesh to fit in unit cube
     */
    void normalizeScale();

    /**
     * @brief Get triangle count
     */
    size_t getTriangleCount() const { return m_cgalMesh.num_faces(); }

    /**
     * @brief Get vertex count
     */
    size_t getVertexCount() const { return m_cgalMesh.num_vertices(); }

    /**
     * @brief Convert to simple vertex/index arrays for rendering
     */
    void toRenderData(std::vector<float>& vertices, std::vector<uint32_t>& indices) const;

  private:
    CGALMesh m_cgalMesh;
    std::vector<VertexAttributes> m_vertexAttributes;
};

} // namespace MoldWing
