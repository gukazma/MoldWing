/*
 *  MoldWing - Screen to Texture Mapper
 *  T6.2.2-5: Map screen coordinates to texture coordinates
 *
 *  This class handles the mapping from screen space to texture space:
 *  1. Get faceId from FacePicker
 *  2. Get face vertices from MeshData
 *  3. Project screen point to 3D ray
 *  4. Calculate ray-triangle intersection for exact hit point
 *  5. Compute barycentric coordinates
 *  6. Interpolate UV using barycentric coordinates
 */

#pragma once

#include "Core/MeshData.hpp"
#include "Render/OrbitCamera.hpp"
#include "Selection/FacePicker.hpp"

#include <memory>
#include <cstdint>

namespace MoldWing
{

/**
 * @brief Result of screen-to-texture mapping
 */
struct TextureMapResult
{
    bool valid = false;           // True if mapping succeeded
    uint32_t faceId = 0xFFFFFFFF; // Face that was hit
    int32_t textureId = -1;       // Texture ID for this face (-1 = no texture)

    // Texture coordinates
    float u = 0.0f;
    float v = 0.0f;

    // Pixel coordinates in texture (computed from u, v and texture dimensions)
    int texX = 0;
    int texY = 0;

    // Barycentric coordinates
    float baryU = 0.0f;
    float baryV = 0.0f;
    float baryW = 0.0f;

    // 3D hit point in world space
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
};

/**
 * @brief Maps screen coordinates to texture coordinates
 *
 * Uses GPU picking (FacePicker) to get the face ID, then performs
 * CPU-side ray-triangle intersection to compute exact texture coordinates.
 */
class ScreenToTextureMapper
{
public:
    ScreenToTextureMapper() = default;

    /**
     * @brief Set the mesh data reference
     * @param mesh Shared pointer to mesh data
     */
    void setMesh(std::shared_ptr<MeshData> mesh) { m_mesh = mesh; }

    /**
     * @brief Map a screen coordinate to texture coordinate
     * @param faceId Face ID from FacePicker (must be valid)
     * @param screenX Screen X coordinate (pixels)
     * @param screenY Screen Y coordinate (pixels)
     * @param camera Current camera
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     * @return TextureMapResult with UV coordinates and other info
     */
    TextureMapResult mapScreenToTexture(
        uint32_t faceId,
        int screenX, int screenY,
        const OrbitCamera& camera,
        int screenWidth, int screenHeight);

    /**
     * @brief Convenience method: pick and map in one call
     * @param picker FacePicker for GPU picking
     * @param pContext Device context
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param camera Current camera
     * @param screenWidth Screen width
     * @param screenHeight Screen height
     * @return TextureMapResult
     */
    TextureMapResult pickAndMap(
        FacePicker& picker,
        Diligent::IDeviceContext* pContext,
        int screenX, int screenY,
        const OrbitCamera& camera,
        int screenWidth, int screenHeight);

private:
    /**
     * @brief Create a ray from screen coordinates
     * @param screenX Screen X
     * @param screenY Screen Y
     * @param camera Camera
     * @param screenWidth Screen width
     * @param screenHeight Screen height
     * @param rayOrigin Output ray origin (camera position)
     * @param rayDir Output ray direction (normalized)
     */
    void screenToRay(
        int screenX, int screenY,
        const OrbitCamera& camera,
        int screenWidth, int screenHeight,
        float* rayOrigin, float* rayDir);

    /**
     * @brief Ray-triangle intersection using Moller-Trumbore algorithm
     * @param rayOrigin Ray origin
     * @param rayDir Ray direction (normalized)
     * @param v0, v1, v2 Triangle vertices
     * @param outT Output: distance along ray
     * @param outU, outV Output: barycentric coordinates
     * @return true if intersection found
     */
    bool rayTriangleIntersect(
        const float* rayOrigin, const float* rayDir,
        const float* v0, const float* v1, const float* v2,
        float& outT, float& outU, float& outV);

    /**
     * @brief Interpolate UV coordinates using barycentric coordinates
     * @param uv0, uv1, uv2 UV coordinates of triangle vertices
     * @param baryU, baryV Barycentric coordinates (w = 1 - u - v)
     * @param outU, outV Output interpolated UV
     */
    void interpolateUV(
        const float* uv0, const float* uv1, const float* uv2,
        float baryU, float baryV,
        float& outU, float& outV);

    // Matrix helper functions
    void invertMatrix4x4(const float* m, float* inv);
    void multiplyMatrix4x4Vec4(const float* m, const float* v, float* result);

    std::shared_ptr<MeshData> m_mesh;
};

} // namespace MoldWing
