/*
 *  MoldWing - Orbit Camera
 *  S1.5/S1.6: Camera system for 3D viewport
 */

#pragma once

#include <cmath>

namespace MoldWing
{

class OrbitCamera
{
public:
    OrbitCamera();

    // Camera transformations
    void rotate(float deltaYaw, float deltaPitch);
    void pan(float deltaX, float deltaY);
    void zoom(float delta);

    // Reset and fit
    void reset();
    void fitToModel(float minX, float minY, float minZ,
                    float maxX, float maxY, float maxZ);

    // Setters
    void setAspectRatio(float aspect) { m_aspectRatio = aspect; }
    void setTarget(float x, float y, float z);

    // Getters
    float getDistance() const { return m_distance; }

    // Get matrices (column-major for DirectX/DiligentEngine)
    void getViewMatrix(float* outMatrix) const;
    void getProjectionMatrix(float* outMatrix) const;

    // Get camera position
    void getPosition(float& x, float& y, float& z) const;

private:
    // Orbit parameters
    float m_targetX = 0.0f;
    float m_targetY = 0.0f;
    float m_targetZ = 0.0f;
    float m_distance = 5.0f;
    float m_yaw = 45.0f;    // degrees
    float m_pitch = 30.0f;  // degrees

    // Projection parameters
    float m_fov = 45.0f;    // degrees
    float m_aspectRatio = 1.77f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    // Constraints
    static constexpr float MIN_PITCH = -89.0f;
    static constexpr float MAX_PITCH = 89.0f;
    static constexpr float MIN_DISTANCE = 0.1f;
    static constexpr float MAX_DISTANCE = 1000.0f;
};

} // namespace MoldWing
