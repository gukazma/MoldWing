/*
 *  MoldWing - Ray Intersection Utilities
 *  CPU-based ray-mesh intersection for accurate picking
 */

#pragma once

#include "MeshData.hpp"
#include <cmath>
#include <limits>

namespace MoldWing
{

/**
 * @brief Ray representation
 */
struct Ray
{
    float originX, originY, originZ;
    float dirX, dirY, dirZ;

    Ray() = default;
    Ray(float ox, float oy, float oz, float dx, float dy, float dz)
        : originX(ox), originY(oy), originZ(oz)
        , dirX(dx), dirY(dy), dirZ(dz)
    {}
};

/**
 * @brief Hit result from ray intersection
 */
struct HitResult
{
    bool hit = false;
    float distance = std::numeric_limits<float>::max();
    float hitX = 0.0f, hitY = 0.0f, hitZ = 0.0f;
    uint32_t faceIndex = 0;
    float u = 0.0f, v = 0.0f;  // Barycentric coordinates
};

/**
 * @brief Ray intersection utilities
 */
class RayIntersection
{
public:
    /**
     * @brief Ray-AABB intersection test (for early rejection)
     * @return true if ray intersects the bounding box
     */
    static bool rayAABB(const Ray& ray, const BoundingBox& box,
                        float& tMin, float& tMax)
    {
        tMin = 0.0f;
        tMax = std::numeric_limits<float>::max();

        for (int i = 0; i < 3; ++i)
        {
            float origin = (i == 0) ? ray.originX : (i == 1) ? ray.originY : ray.originZ;
            float dir = (i == 0) ? ray.dirX : (i == 1) ? ray.dirY : ray.dirZ;
            float bmin = box.min[i];
            float bmax = box.max[i];

            if (std::abs(dir) < 1e-8f)
            {
                // Ray is parallel to slab
                if (origin < bmin || origin > bmax)
                    return false;
            }
            else
            {
                float invD = 1.0f / dir;
                float t0 = (bmin - origin) * invD;
                float t1 = (bmax - origin) * invD;

                if (invD < 0.0f)
                    std::swap(t0, t1);

                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);

                if (tMax < tMin)
                    return false;
            }
        }
        return true;
    }

    /**
     * @brief Ray-triangle intersection using Möller–Trumbore algorithm
     * @param ray The ray
     * @param v0, v1, v2 Triangle vertices
     * @param t Output: distance along ray
     * @param u, v Output: barycentric coordinates
     * @return true if intersection found
     */
    static bool rayTriangle(const Ray& ray,
                            float v0x, float v0y, float v0z,
                            float v1x, float v1y, float v1z,
                            float v2x, float v2y, float v2z,
                            float& t, float& u, float& v)
    {
        constexpr float EPSILON = 1e-8f;

        // Edge vectors
        float e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
        float e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;

        // Cross product: h = ray.dir x e2
        float hx = ray.dirY * e2z - ray.dirZ * e2y;
        float hy = ray.dirZ * e2x - ray.dirX * e2z;
        float hz = ray.dirX * e2y - ray.dirY * e2x;

        // Dot product: a = e1 . h
        float a = e1x * hx + e1y * hy + e1z * hz;

        // Ray parallel to triangle
        if (std::abs(a) < EPSILON)
            return false;

        float f = 1.0f / a;

        // Vector from v0 to ray origin
        float sx = ray.originX - v0x;
        float sy = ray.originY - v0y;
        float sz = ray.originZ - v0z;

        // Barycentric u coordinate
        u = f * (sx * hx + sy * hy + sz * hz);
        if (u < 0.0f || u > 1.0f)
            return false;

        // Cross product: q = s x e1
        float qx = sy * e1z - sz * e1y;
        float qy = sz * e1x - sx * e1z;
        float qz = sx * e1y - sy * e1x;

        // Barycentric v coordinate
        v = f * (ray.dirX * qx + ray.dirY * qy + ray.dirZ * qz);
        if (v < 0.0f || u + v > 1.0f)
            return false;

        // Distance along ray
        t = f * (e2x * qx + e2y * qy + e2z * qz);

        // Only count intersections in front of ray
        return t > EPSILON;
    }

    /**
     * @brief Find closest intersection of ray with mesh
     * @param ray The ray
     * @param mesh The mesh to intersect
     * @param result Output: hit result
     * @return true if any intersection found
     */
    static bool rayMesh(const Ray& ray, const MeshData& mesh, HitResult& result)
    {
        result.hit = false;
        result.distance = std::numeric_limits<float>::max();

        // Early rejection with bounding box
        float tMin, tMax;
        if (!rayAABB(ray, mesh.bounds, tMin, tMax))
            return false;

        // Check all triangles
        const uint32_t numFaces = mesh.faceCount();
        for (uint32_t i = 0; i < numFaces; ++i)
        {
            uint32_t i0 = mesh.indices[i * 3 + 0];
            uint32_t i1 = mesh.indices[i * 3 + 1];
            uint32_t i2 = mesh.indices[i * 3 + 2];

            const Vertex& v0 = mesh.vertices[i0];
            const Vertex& v1 = mesh.vertices[i1];
            const Vertex& v2 = mesh.vertices[i2];

            float t, u, v;
            if (rayTriangle(ray,
                           v0.position[0], v0.position[1], v0.position[2],
                           v1.position[0], v1.position[1], v1.position[2],
                           v2.position[0], v2.position[1], v2.position[2],
                           t, u, v))
            {
                if (t < result.distance)
                {
                    result.hit = true;
                    result.distance = t;
                    result.faceIndex = i;
                    result.u = u;
                    result.v = v;

                    // Calculate hit point
                    result.hitX = ray.originX + ray.dirX * t;
                    result.hitY = ray.originY + ray.dirY * t;
                    result.hitZ = ray.originZ + ray.dirZ * t;
                }
            }
        }

        return result.hit;
    }
};

} // namespace MoldWing
