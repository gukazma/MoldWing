/*
 *  MoldWing - Orbit Camera Implementation
 *  S1.5/S1.6: Camera system
 */

#include "OrbitCamera.hpp"
#include <cmath>
#include <algorithm>

namespace MoldWing
{

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
}

OrbitCamera::OrbitCamera()
{
    reset();
}

void OrbitCamera::rotate(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;

    // Wrap yaw
    while (m_yaw > 360.0f) m_yaw -= 360.0f;
    while (m_yaw < 0.0f) m_yaw += 360.0f;

    // Clamp pitch
    m_pitch = std::clamp(m_pitch, MIN_PITCH, MAX_PITCH);
}

void OrbitCamera::pan(float deltaX, float deltaY)
{
    float yawRad = m_yaw * DEG_TO_RAD;

    // Calculate right and up vectors in world space
    float rightX = std::cos(yawRad);
    float rightZ = std::sin(yawRad);

    // Pan in camera-local XY plane
    float panScale = m_distance * 0.1f;
    m_targetX -= rightX * deltaX * panScale;
    m_targetZ -= rightZ * deltaX * panScale;
    m_targetY += deltaY * panScale;
}

void OrbitCamera::zoom(float delta)
{
    m_distance *= std::pow(0.9f, delta);
    m_distance = std::clamp(m_distance, MIN_DISTANCE, MAX_DISTANCE);
}

void OrbitCamera::reset()
{
    m_targetX = m_targetY = m_targetZ = 0.0f;
    m_distance = 5.0f;
    m_yaw = 45.0f;
    m_pitch = 30.0f;
}

void OrbitCamera::fitToModel(float minX, float minY, float minZ,
                              float maxX, float maxY, float maxZ)
{
    // Center target on model
    m_targetX = (minX + maxX) * 0.5f;
    m_targetY = (minY + maxY) * 0.5f;
    m_targetZ = (minZ + maxZ) * 0.5f;

    // Calculate distance to fit model in view
    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    float diagonal = std::sqrt(dx*dx + dy*dy + dz*dz);

    float fovRad = m_fov * DEG_TO_RAD;
    m_distance = (diagonal * 0.5f) / std::tan(fovRad * 0.5f) * 1.5f;
    m_distance = std::clamp(m_distance, MIN_DISTANCE, MAX_DISTANCE);
}

void OrbitCamera::setTarget(float x, float y, float z)
{
    m_targetX = x;
    m_targetY = y;
    m_targetZ = z;
}

void OrbitCamera::getPosition(float& x, float& y, float& z) const
{
    float yawRad = m_yaw * DEG_TO_RAD;
    float pitchRad = m_pitch * DEG_TO_RAD;

    float cosPitch = std::cos(pitchRad);
    x = m_targetX + m_distance * cosPitch * std::sin(yawRad);
    y = m_targetY + m_distance * std::sin(pitchRad);
    z = m_targetZ + m_distance * cosPitch * std::cos(yawRad);
}

void OrbitCamera::getViewMatrix(float* m) const
{
    float posX, posY, posZ;
    getPosition(posX, posY, posZ);

    // Look direction
    float lookX = m_targetX - posX;
    float lookY = m_targetY - posY;
    float lookZ = m_targetZ - posZ;

    // Normalize
    float len = std::sqrt(lookX*lookX + lookY*lookY + lookZ*lookZ);
    lookX /= len; lookY /= len; lookZ /= len;

    // Up vector (world Y)
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

    // Right = look x up
    float rightX = lookY * upZ - lookZ * upY;
    float rightY = lookZ * upX - lookX * upZ;
    float rightZ = lookX * upY - lookY * upX;

    // Normalize right
    len = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
    rightX /= len; rightY /= len; rightZ /= len;

    // Recalculate up = right x look
    upX = rightY * lookZ - rightZ * lookY;
    upY = rightZ * lookX - rightX * lookZ;
    upZ = rightX * lookY - rightY * lookX;

    // Build view matrix (column-major)
    // Row 0: right
    m[0] = rightX;  m[4] = rightY;  m[8]  = rightZ;  m[12] = -(rightX*posX + rightY*posY + rightZ*posZ);
    // Row 1: up
    m[1] = upX;     m[5] = upY;     m[9]  = upZ;     m[13] = -(upX*posX + upY*posY + upZ*posZ);
    // Row 2: -look (forward into screen)
    m[2] = -lookX;  m[6] = -lookY;  m[10] = -lookZ;  m[14] = (lookX*posX + lookY*posY + lookZ*posZ);
    // Row 3
    m[3] = 0.0f;    m[7] = 0.0f;    m[11] = 0.0f;    m[15] = 1.0f;
}

void OrbitCamera::getProjectionMatrix(float* m) const
{
    float fovRad = m_fov * DEG_TO_RAD;
    float tanHalfFov = std::tan(fovRad * 0.5f);

    float yScale = 1.0f / tanHalfFov;
    float xScale = yScale / m_aspectRatio;
    float zRange = m_farPlane - m_nearPlane;

    // Perspective projection (column-major, reversed-Z for better depth precision)
    m[0] = xScale;  m[4] = 0.0f;    m[8]  = 0.0f;                               m[12] = 0.0f;
    m[1] = 0.0f;    m[5] = yScale;  m[9]  = 0.0f;                               m[13] = 0.0f;
    m[2] = 0.0f;    m[6] = 0.0f;    m[10] = m_nearPlane / zRange;               m[14] = m_farPlane * m_nearPlane / zRange;
    m[3] = 0.0f;    m[7] = 0.0f;    m[11] = -1.0f;                              m[15] = 0.0f;
}

} // namespace MoldWing
