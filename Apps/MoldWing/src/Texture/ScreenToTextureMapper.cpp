/*
 *  MoldWing - Screen to Texture Mapper Implementation
 *  T6.2.2-5: Map screen coordinates to texture coordinates
 */

#include "ScreenToTextureMapper.hpp"
#include "Core/Logger.hpp"

#include <cmath>
#include <algorithm>

namespace MoldWing
{

TextureMapResult ScreenToTextureMapper::mapScreenToTexture(
    uint32_t faceId,
    int screenX, int screenY,
    const OrbitCamera& camera,
    int screenWidth, int screenHeight)
{
    TextureMapResult result;

    if (!m_mesh || faceId >= m_mesh->faceCount())
    {
        return result;
    }

    // Get face vertices
    uint32_t idx0 = m_mesh->indices[faceId * 3 + 0];
    uint32_t idx1 = m_mesh->indices[faceId * 3 + 1];
    uint32_t idx2 = m_mesh->indices[faceId * 3 + 2];

    const Vertex& v0 = m_mesh->vertices[idx0];
    const Vertex& v1 = m_mesh->vertices[idx1];
    const Vertex& v2 = m_mesh->vertices[idx2];

    // Create ray from screen coordinates
    float rayOrigin[3], rayDir[3];
    screenToRay(screenX, screenY, camera, screenWidth, screenHeight, rayOrigin, rayDir);

    // Ray-triangle intersection
    float t, baryU, baryV;
    if (!rayTriangleIntersect(rayOrigin, rayDir, v0.position, v1.position, v2.position, t, baryU, baryV))
    {
        // This shouldn't happen if faceId is valid, but handle gracefully
        // Use center of triangle as fallback
        baryU = 1.0f / 3.0f;
        baryV = 1.0f / 3.0f;
    }

    float baryW = 1.0f - baryU - baryV;

    // Interpolate UV coordinates
    float u, v;
    interpolateUV(v0.texcoord, v1.texcoord, v2.texcoord, baryU, baryV, u, v);

    // Calculate 3D hit point
    result.worldX = baryW * v0.position[0] + baryU * v1.position[0] + baryV * v2.position[0];
    result.worldY = baryW * v0.position[1] + baryU * v1.position[1] + baryV * v2.position[1];
    result.worldZ = baryW * v0.position[2] + baryU * v1.position[2] + baryV * v2.position[2];

    // Fill result
    result.valid = true;
    result.faceId = faceId;
    result.u = u;
    result.v = v;
    result.baryU = baryU;
    result.baryV = baryV;
    result.baryW = baryW;

    // Get texture ID from material
    if (faceId < m_mesh->faceMaterialIds.size())
    {
        uint32_t matId = m_mesh->faceMaterialIds[faceId];
        if (matId < m_mesh->materials.size())
        {
            result.textureId = m_mesh->materials[matId].textureId;
        }
    }

    // Calculate pixel coordinates in texture
    if (result.textureId >= 0 && result.textureId < static_cast<int>(m_mesh->textures.size()))
    {
        auto& tex = m_mesh->textures[result.textureId];
        if (tex && tex->isValid())
        {
            // Handle UV wrapping (fmod handles negative values correctly with adjustment)
            float wrappedU = u - std::floor(u);
            float wrappedV = v - std::floor(v);

            result.texX = static_cast<int>(wrappedU * tex->width());
            result.texY = static_cast<int>(wrappedV * tex->height());

            // Clamp to texture bounds
            result.texX = std::clamp(result.texX, 0, tex->width() - 1);
            result.texY = std::clamp(result.texY, 0, tex->height() - 1);
        }
    }

    return result;
}

TextureMapResult ScreenToTextureMapper::pickAndMap(
    FacePicker& picker,
    Diligent::IDeviceContext* pContext,
    int screenX, int screenY,
    const OrbitCamera& camera,
    int screenWidth, int screenHeight)
{
    TextureMapResult result;

    // Use FacePicker to get face ID
    PickResult pickResult = picker.pickPoint(pContext, screenX, screenY);

    if (!pickResult.hit)
    {
        return result;
    }

    // Map to texture coordinates
    return mapScreenToTexture(pickResult.faceId, screenX, screenY, camera, screenWidth, screenHeight);
}

void ScreenToTextureMapper::screenToRay(
    int screenX, int screenY,
    const OrbitCamera& camera,
    int screenWidth, int screenHeight,
    float* rayOrigin, float* rayDir)
{
    // Get camera position as ray origin
    camera.getPosition(rayOrigin[0], rayOrigin[1], rayOrigin[2]);

    // Convert screen coordinates to NDC [-1, 1]
    float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY / screenHeight);  // Flip Y

    // Get view and projection matrices
    float view[16], proj[16];
    camera.getViewMatrix(view);
    camera.getProjectionMatrix(proj);

    // Invert projection matrix
    float invProj[16];
    invertMatrix4x4(proj, invProj);

    // Invert view matrix
    float invView[16];
    invertMatrix4x4(view, invView);

    // Transform NDC point to view space
    float viewPoint[4] = {ndcX, ndcY, -1.0f, 1.0f};
    float viewDir[4];
    multiplyMatrix4x4Vec4(invProj, viewPoint, viewDir);

    // Perspective divide
    if (std::abs(viewDir[3]) > 1e-6f)
    {
        viewDir[0] /= viewDir[3];
        viewDir[1] /= viewDir[3];
        viewDir[2] /= viewDir[3];
    }
    viewDir[3] = 0.0f;  // Direction vector

    // Transform to world space
    float worldDir[4];
    multiplyMatrix4x4Vec4(invView, viewDir, worldDir);

    // Normalize
    float len = std::sqrt(worldDir[0] * worldDir[0] + worldDir[1] * worldDir[1] + worldDir[2] * worldDir[2]);
    if (len > 1e-6f)
    {
        rayDir[0] = worldDir[0] / len;
        rayDir[1] = worldDir[1] / len;
        rayDir[2] = worldDir[2] / len;
    }
    else
    {
        rayDir[0] = 0.0f;
        rayDir[1] = 0.0f;
        rayDir[2] = -1.0f;
    }
}

bool ScreenToTextureMapper::rayTriangleIntersect(
    const float* rayOrigin, const float* rayDir,
    const float* v0, const float* v1, const float* v2,
    float& outT, float& outU, float& outV)
{
    // Moller-Trumbore algorithm
    const float EPSILON = 1e-7f;

    // Edge vectors
    float edge1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
    float edge2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};

    // Cross product of ray direction and edge2
    float h[3];
    h[0] = rayDir[1] * edge2[2] - rayDir[2] * edge2[1];
    h[1] = rayDir[2] * edge2[0] - rayDir[0] * edge2[2];
    h[2] = rayDir[0] * edge2[1] - rayDir[1] * edge2[0];

    // Dot product of edge1 and h
    float a = edge1[0] * h[0] + edge1[1] * h[1] + edge1[2] * h[2];

    if (a > -EPSILON && a < EPSILON)
        return false;  // Ray is parallel to triangle

    float f = 1.0f / a;

    // Vector from v0 to ray origin
    float s[3] = {rayOrigin[0] - v0[0], rayOrigin[1] - v0[1], rayOrigin[2] - v0[2]};

    // Calculate u parameter
    outU = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
    if (outU < 0.0f || outU > 1.0f)
        return false;

    // Cross product of s and edge1
    float q[3];
    q[0] = s[1] * edge1[2] - s[2] * edge1[1];
    q[1] = s[2] * edge1[0] - s[0] * edge1[2];
    q[2] = s[0] * edge1[1] - s[1] * edge1[0];

    // Calculate v parameter
    outV = f * (rayDir[0] * q[0] + rayDir[1] * q[1] + rayDir[2] * q[2]);
    if (outV < 0.0f || outU + outV > 1.0f)
        return false;

    // Calculate t (distance along ray)
    outT = f * (edge2[0] * q[0] + edge2[1] * q[1] + edge2[2] * q[2]);

    return outT > EPSILON;  // Intersection is in front of ray origin
}

void ScreenToTextureMapper::interpolateUV(
    const float* uv0, const float* uv1, const float* uv2,
    float baryU, float baryV,
    float& outU, float& outV)
{
    // Barycentric interpolation: P = w*v0 + u*v1 + v*v2
    // where w = 1 - u - v
    float baryW = 1.0f - baryU - baryV;

    outU = baryW * uv0[0] + baryU * uv1[0] + baryV * uv2[0];
    outV = baryW * uv0[1] + baryU * uv1[1] + baryV * uv2[1];
}

// Helper: Invert 4x4 matrix (simple implementation)
void ScreenToTextureMapper::invertMatrix4x4(const float* m, float* inv)
{
    float det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] -
             m[5]  * m[11] * m[14] -
             m[9]  * m[6]  * m[15] +
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] -
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
              m[4]  * m[11] * m[14] +
              m[8]  * m[6]  * m[15] -
              m[8]  * m[7]  * m[14] -
              m[12] * m[6]  * m[11] +
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
             m[4]  * m[11] * m[13] -
             m[8]  * m[5] * m[15] +
             m[8]  * m[7] * m[13] +
             m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] -
               m[8]  * m[6] * m[13] -
               m[12] * m[5] * m[10] +
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
              m[1]  * m[11] * m[14] +
              m[9]  * m[2] * m[15] -
              m[9]  * m[3] * m[14] -
              m[13] * m[2] * m[11] +
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
             m[0]  * m[11] * m[14] -
             m[8]  * m[2] * m[15] +
             m[8]  * m[3] * m[14] +
             m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
              m[0]  * m[11] * m[13] +
              m[8]  * m[1] * m[15] -
              m[8]  * m[3] * m[13] -
              m[12] * m[1] * m[11] +
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
              m[0]  * m[10] * m[13] -
              m[8]  * m[1] * m[14] +
              m[8]  * m[2] * m[13] +
              m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
             m[1]  * m[7] * m[14] -
             m[5]  * m[2] * m[15] +
             m[5]  * m[3] * m[14] +
             m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
              m[0]  * m[7] * m[14] +
              m[4]  * m[2] * m[15] -
              m[4]  * m[3] * m[14] -
              m[12] * m[2] * m[7] +
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
              m[0]  * m[7] * m[13] -
              m[4]  * m[1] * m[15] +
              m[4]  * m[3] * m[13] +
              m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
               m[0]  * m[6] * m[13] +
               m[4]  * m[1] * m[14] -
               m[4]  * m[2] * m[13] -
               m[12] * m[1] * m[6] +
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
              m[1] * m[7] * m[10] +
              m[5] * m[2] * m[11] -
              m[5] * m[3] * m[10] -
              m[9] * m[2] * m[7] +
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
             m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
               m[0] * m[7] * m[9] +
               m[4] * m[1] * m[11] -
               m[4] * m[3] * m[9] -
               m[8] * m[1] * m[7] +
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
              m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (std::abs(det) < 1e-10f)
    {
        // Matrix is singular, return identity
        std::fill(inv, inv + 16, 0.0f);
        inv[0] = inv[5] = inv[10] = inv[15] = 1.0f;
        return;
    }

    det = 1.0f / det;

    for (i = 0; i < 16; i++)
        inv[i] = inv[i] * det;
}

// Helper: Multiply 4x4 matrix by 4-vector
void ScreenToTextureMapper::multiplyMatrix4x4Vec4(const float* m, const float* v, float* result)
{
    result[0] = m[0] * v[0] + m[4] * v[1] + m[8]  * v[2] + m[12] * v[3];
    result[1] = m[1] * v[0] + m[5] * v[1] + m[9]  * v[2] + m[13] * v[3];
    result[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
    result[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];
}

} // namespace MoldWing
