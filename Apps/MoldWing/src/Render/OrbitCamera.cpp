/*
 *  MoldWing - Orbit Camera Implementation (Enhanced)
 *  Professional camera system with smoothing, inertia, and view presets
 */

#include "OrbitCamera.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace MoldWing
{

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;

    // Easing function for smooth animations
    float easeOutCubic(float t)
    {
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }

    // Smooth exponential interpolation
    float expDecay(float current, float target, float speed, float dt)
    {
        return target + (current - target) * std::exp(-speed * dt);
    }

    // Normalize angle to 0-360 range
    float normalizeAngle(float angle)
    {
        while (angle > 360.0f) angle -= 360.0f;
        while (angle < 0.0f) angle += 360.0f;
        return angle;
    }

    // Shortest angular distance for smooth interpolation
    float shortestAngleDist(float from, float to)
    {
        float diff = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
        return diff;
    }
}

// =============================================================================
// Constructor
// =============================================================================

OrbitCamera::OrbitCamera()
{
    // Initialize states with defaults
    m_currentState = CameraState{};
    m_targetState = CameraState{};
    m_animStartState = CameraState{};
    m_animEndState = CameraState{};

    m_lastUpdateTime = std::chrono::steady_clock::now();
}

// =============================================================================
// Core Update
// =============================================================================

void OrbitCamera::update(float deltaTime)
{
    // Handle first frame
    if (m_firstUpdate)
    {
        m_lastUpdateTime = std::chrono::steady_clock::now();
        m_firstUpdate = false;
        m_currentState = m_targetState;
        return;
    }

    // Update subsystems in order
    updateAnimation(deltaTime);
    updateInertia(deltaTime);
    updateSmoothing(deltaTime);
}

void OrbitCamera::updateSmoothing(float deltaTime)
{
    if (!m_settings.enableSmoothing)
    {
        m_currentState = m_targetState;
        return;
    }

    float speed = m_settings.smoothSpeed;

    // Smooth interpolation for angles (handle wrap-around)
    float yawDiff = shortestAngleDist(m_currentState.yaw, m_targetState.yaw);
    m_currentState.yaw = expDecay(m_currentState.yaw, m_currentState.yaw + yawDiff, speed, deltaTime);
    m_currentState.yaw = normalizeAngle(m_currentState.yaw);

    m_currentState.pitch = expDecay(m_currentState.pitch, m_targetState.pitch, speed, deltaTime);

    // Smooth interpolation for distance and position
    m_currentState.distance = expDecay(m_currentState.distance, m_targetState.distance, speed, deltaTime);
    m_currentState.targetX = expDecay(m_currentState.targetX, m_targetState.targetX, speed, deltaTime);
    m_currentState.targetY = expDecay(m_currentState.targetY, m_targetState.targetY, speed, deltaTime);
    m_currentState.targetZ = expDecay(m_currentState.targetZ, m_targetState.targetZ, speed, deltaTime);

    // Orthographic scale
    m_currentState.orthoScale = expDecay(m_currentState.orthoScale, m_targetState.orthoScale, speed, deltaTime);

    // Orthographic mode is instant
    m_currentState.orthographic = m_targetState.orthographic;
}

void OrbitCamera::updateInertia(float deltaTime)
{
    if (!m_settings.enableInertia)
    {
        m_yawVelocity = m_pitchVelocity = 0.0f;
        m_panVelocityX = m_panVelocityY = 0.0f;
        m_zoomVelocity = 0.0f;
        return;
    }

    // Only apply inertia when not dragging
    if (!m_isDraggingRotate && !m_isDraggingPan)
    {
        // Apply rotation inertia
        if (std::abs(m_yawVelocity) > m_settings.minVelocity ||
            std::abs(m_pitchVelocity) > m_settings.minVelocity)
        {
            m_targetState.yaw += m_yawVelocity * deltaTime * 60.0f;
            m_targetState.yaw = normalizeAngle(m_targetState.yaw);

            m_targetState.pitch += m_pitchVelocity * deltaTime * 60.0f;
            m_targetState.pitch = applyPitchConstraint(m_targetState.pitch);

            // Damping
            float damping = std::pow(m_settings.inertiaDamping, deltaTime * 60.0f);
            m_yawVelocity *= damping;
            m_pitchVelocity *= damping;
        }
        else
        {
            m_yawVelocity = m_pitchVelocity = 0.0f;
        }

        // Apply pan inertia
        if (std::abs(m_panVelocityX) > m_settings.minVelocity ||
            std::abs(m_panVelocityY) > m_settings.minVelocity)
        {
            float yawRad = m_targetState.yaw * DEG_TO_RAD;
            float pitchRad = m_targetState.pitch * DEG_TO_RAD;

            float cosYaw = std::cos(yawRad);
            float sinYaw = std::sin(yawRad);
            float cosPitch = std::cos(pitchRad);
            float sinPitch = std::sin(pitchRad);

            // Camera forward direction (Z-up system)
            float fwdX = -cosPitch * sinYaw;
            float fwdY = -cosPitch * cosYaw;
            float fwdZ = -sinPitch;

            // World up is Z
            float worldUpX = 0.0f, worldUpY = 0.0f, worldUpZ = 1.0f;

            // Right vector = forward x worldUp
            float rightX = fwdY * worldUpZ - fwdZ * worldUpY;
            float rightY = fwdZ * worldUpX - fwdX * worldUpZ;
            float rightZ = fwdX * worldUpY - fwdY * worldUpX;

            // Normalize right
            float rightLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
            if (rightLen > 0.0001f)
            {
                rightX /= rightLen; rightY /= rightLen; rightZ /= rightLen;
            }

            // Up vector = right x forward
            float upX = rightY * fwdZ - rightZ * fwdY;
            float upY = rightZ * fwdX - rightX * fwdZ;
            float upZ = rightX * fwdY - rightY * fwdX;

            // Normalize up
            float upLen = std::sqrt(upX*upX + upY*upY + upZ*upZ);
            if (upLen > 0.0001f)
            {
                upX /= upLen; upY /= upLen; upZ /= upLen;
            }

            // Velocities are already in world-space, just apply with time factor
            float velX = m_panVelocityX * deltaTime * 60.0f;
            float velY = m_panVelocityY * deltaTime * 60.0f;

            m_targetState.targetX -= (rightX * velX + upX * velY);
            m_targetState.targetY -= (rightY * velX + upY * velY);
            m_targetState.targetZ -= (rightZ * velX + upZ * velY);

            // Damping
            float damping = std::pow(m_settings.inertiaDamping, deltaTime * 60.0f);
            m_panVelocityX *= damping;
            m_panVelocityY *= damping;
        }
        else
        {
            m_panVelocityX = m_panVelocityY = 0.0f;
        }
    }

    // Zoom inertia (always active)
    if (std::abs(m_zoomVelocity) > m_settings.minVelocity)
    {
        m_targetState.distance *= std::pow(0.99f, m_zoomVelocity * deltaTime * 60.0f);
        m_targetState.distance = std::clamp(m_targetState.distance,
                                             m_settings.minDistance,
                                             m_settings.maxDistance);

        float damping = std::pow(m_settings.inertiaDamping, deltaTime * 60.0f);
        m_zoomVelocity *= damping;
    }
    else
    {
        m_zoomVelocity = 0.0f;
    }
}

void OrbitCamera::updateAnimation(float deltaTime)
{
    if (!m_isAnimating)
        return;

    m_animationTime += deltaTime;

    float t = m_animationTime / m_animationDuration;

    if (t >= 1.0f)
    {
        // Animation complete
        m_targetState = m_animEndState;
        m_isAnimating = false;
        m_animationTime = 0.0f;
        return;
    }

    // Apply easing
    t = easeOutCubic(t);

    // Interpolate state
    m_targetState = CameraState::lerp(m_animStartState, m_animEndState, t);
}

// =============================================================================
// Input Handling - Rotation
// =============================================================================

void OrbitCamera::beginRotate()
{
    m_isDraggingRotate = true;
    m_lastDeltaYaw = 0.0f;
    m_lastDeltaPitch = 0.0f;

    // Stop any ongoing animation - it would overwrite target state
    m_isAnimating = false;
    m_animationTime = 0.0f;

    // Stop any ongoing inertia (both rotation and pan)
    m_yawVelocity = 0.0f;
    m_pitchVelocity = 0.0f;
    m_panVelocityX = 0.0f;
    m_panVelocityY = 0.0f;

    // Sync target position with current to prevent smoothing-induced panning
    m_targetState.targetX = m_currentState.targetX;
    m_targetState.targetY = m_currentState.targetY;
    m_targetState.targetZ = m_currentState.targetZ;

    // Also sync angles, distance, and orthoScale to prevent ANY smoothing movement
    m_targetState.yaw = m_currentState.yaw;
    m_targetState.pitch = m_currentState.pitch;
    m_targetState.distance = m_currentState.distance;
    m_targetState.orthoScale = m_currentState.orthoScale;
}

void OrbitCamera::endRotate()
{
    m_isDraggingRotate = false;

    // Transfer last movement to velocity for inertia
    if (m_settings.enableInertia)
    {
        m_yawVelocity = m_lastDeltaYaw;
        m_pitchVelocity = m_lastDeltaPitch;
    }
}

void OrbitCamera::rotate(float deltaYaw, float deltaPitch, RotationConstraint constraint)
{
    // Apply sensitivity
    deltaYaw *= m_settings.rotationSensitivity;
    deltaPitch *= m_settings.rotationSensitivity;

    // Invert Y if needed
    if (m_settings.invertY)
        deltaPitch = -deltaPitch;

    // Apply constraints
    switch (constraint)
    {
        case RotationConstraint::HorizontalOnly:
            deltaPitch = 0.0f;
            break;
        case RotationConstraint::VerticalOnly:
            deltaYaw = 0.0f;
            break;
        case RotationConstraint::Snap45:
        case RotationConstraint::Snap90:
            // Snap will be applied after accumulating movement
            break;
        default:
            break;
    }

    // Update target state
    // In Z-up system, positive yaw = counter-clockwise when viewed from above
    m_targetState.yaw += deltaYaw;
    m_targetState.yaw = applyYawConstraint(m_targetState.yaw);

    m_targetState.pitch += deltaPitch;
    m_targetState.pitch = applyPitchConstraint(m_targetState.pitch);

    // Apply snapping if requested
    if (constraint == RotationConstraint::Snap45)
    {
        m_targetState.yaw = snapAngle(m_targetState.yaw, 45.0f);
        m_targetState.pitch = snapAngle(m_targetState.pitch, 45.0f);
    }
    else if (constraint == RotationConstraint::Snap90)
    {
        m_targetState.yaw = snapAngle(m_targetState.yaw, 90.0f);
        m_targetState.pitch = snapAngle(m_targetState.pitch, 90.0f);
    }

    // Store for inertia
    m_lastDeltaYaw = deltaYaw;
    m_lastDeltaPitch = deltaPitch;
}

// =============================================================================
// Input Handling - Pan
// =============================================================================

void OrbitCamera::beginPan()
{
    m_isDraggingPan = true;
    m_lastDeltaPanX = 0.0f;
    m_lastDeltaPanY = 0.0f;

    // Stop any ongoing animation - it would overwrite target state
    m_isAnimating = false;
    m_animationTime = 0.0f;

    // Stop any ongoing inertia (both pan and rotation)
    m_panVelocityX = 0.0f;
    m_panVelocityY = 0.0f;
    m_yawVelocity = 0.0f;
    m_pitchVelocity = 0.0f;

    // Sync ALL target state with current to prevent ANY smoothing-induced movement
    m_targetState.targetX = m_currentState.targetX;
    m_targetState.targetY = m_currentState.targetY;
    m_targetState.targetZ = m_currentState.targetZ;
    m_targetState.yaw = m_currentState.yaw;
    m_targetState.pitch = m_currentState.pitch;
    m_targetState.distance = m_currentState.distance;
    m_targetState.orthoScale = m_currentState.orthoScale;
}

void OrbitCamera::endPan()
{
    m_isDraggingPan = false;

    // Transfer last movement to velocity for inertia
    if (m_settings.enableInertia)
    {
        m_panVelocityX = m_lastDeltaPanX;
        m_panVelocityY = m_lastDeltaPanY;
    }
}

void OrbitCamera::pan(float pixelDeltaX, float pixelDeltaY, int viewportWidth, int viewportHeight)
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
        return;

    // Calculate world-space size of viewport at target distance
    // For perspective: height = 2 * distance * tan(fov/2)
    // For orthographic: use orthoScale directly
    float worldHeight, worldWidth;

    if (m_targetState.orthographic)
    {
        worldHeight = m_targetState.orthoScale * 2.0f;
        worldWidth = worldHeight * m_aspectRatio;
    }
    else
    {
        float halfFovRad = (m_settings.fov * 0.5f) * DEG_TO_RAD;
        worldHeight = 2.0f * m_targetState.distance * std::tan(halfFovRad);
        worldWidth = worldHeight * m_aspectRatio;
    }

    // Convert pixel movement to world-space movement (1:1 mapping)
    float worldDeltaX = (pixelDeltaX / static_cast<float>(viewportWidth)) * worldWidth;
    float worldDeltaY = (pixelDeltaY / static_cast<float>(viewportHeight)) * worldHeight;

    // Apply sensitivity (default 1.0 means 1:1)
    worldDeltaX *= m_settings.panSensitivity;
    worldDeltaY *= m_settings.panSensitivity;

    float yawRad = m_targetState.yaw * DEG_TO_RAD;
    float pitchRad = m_targetState.pitch * DEG_TO_RAD;

    // Calculate camera basis vectors for proper view-plane panning (Z-up)
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);
    float cosPitch = std::cos(pitchRad);
    float sinPitch = std::sin(pitchRad);

    // Camera forward direction (from camera to target) - Z-up system
    float fwdX = -cosPitch * sinYaw;
    float fwdY = -cosPitch * cosYaw;
    float fwdZ = -sinPitch;

    // World up is Z
    float worldUpX = 0.0f, worldUpY = 0.0f, worldUpZ = 1.0f;

    // Right vector = forward x worldUp
    float rightX = fwdY * worldUpZ - fwdZ * worldUpY;
    float rightY = fwdZ * worldUpX - fwdX * worldUpZ;
    float rightZ = fwdX * worldUpY - fwdY * worldUpX;

    // Normalize right
    float rightLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
    if (rightLen > 0.0001f)
    {
        rightX /= rightLen;
        rightY /= rightLen;
        rightZ /= rightLen;
    }

    // Up vector = right x forward (perpendicular to both, in view plane)
    float upX = rightY * fwdZ - rightZ * fwdY;
    float upY = rightZ * fwdX - rightX * fwdZ;
    float upZ = rightX * fwdY - rightY * fwdX;

    // Normalize up vector
    float upLen = std::sqrt(upX*upX + upY*upY + upZ*upZ);
    if (upLen > 0.0001f)
    {
        upX /= upLen;
        upY /= upLen;
        upZ /= upLen;
    }

    // Move target in the view plane (opposite to mouse movement for natural feel)
    m_targetState.targetX -= rightX * worldDeltaX + upX * worldDeltaY;
    m_targetState.targetY -= rightY * worldDeltaX + upY * worldDeltaY;
    m_targetState.targetZ -= rightZ * worldDeltaX + upZ * worldDeltaY;

    // Store normalized delta for inertia
    m_lastDeltaPanX = worldDeltaX;
    m_lastDeltaPanY = worldDeltaY;
}

// =============================================================================
// Input Handling - Zoom
// =============================================================================

void OrbitCamera::zoom(float delta, float cursorX, float cursorY)
{
    // Direct zoom without sensitivity reduction for responsive feel
    // delta is typically ~1.0 per scroll tick
    // Negate delta: scroll up = zoom in (closer), scroll down = zoom out (farther)
    float zoomFactor = std::pow(0.85f, -delta);  // ~15% per scroll tick
    float oldDistance = m_targetState.distance;
    float newDistance = oldDistance * zoomFactor;
    
    // Clamp distance
    newDistance = std::clamp(newDistance, m_settings.minDistance, m_settings.maxDistance);
    
    // Zoom to cursor: keep the point under cursor stationary
    if (m_settings.zoomToCursor)
    {
        // Convert cursor to NDC (-1 to 1)
        float ndcX = (cursorX * 2.0f - 1.0f);
        float ndcY = (1.0f - cursorY * 2.0f);

        // Get camera position and orientation
        float yawRad = m_targetState.yaw * DEG_TO_RAD;
        float pitchRad = m_targetState.pitch * DEG_TO_RAD;

        float cosYaw = std::cos(yawRad);
        float sinYaw = std::sin(yawRad);
        float cosPitch = std::cos(pitchRad);
        float sinPitch = std::sin(pitchRad);

        // Camera forward direction (Z-up system)
        float fwdX = -cosPitch * sinYaw;
        float fwdY = -cosPitch * cosYaw;
        float fwdZ = -sinPitch;

        // World up is Z
        float worldUpX = 0.0f, worldUpY = 0.0f, worldUpZ = 1.0f;

        // Right vector = forward x worldUp
        float rightX = fwdY * worldUpZ - fwdZ * worldUpY;
        float rightY = fwdZ * worldUpX - fwdX * worldUpZ;
        float rightZ = fwdX * worldUpY - fwdY * worldUpX;

        // Normalize right
        float rightLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
        if (rightLen > 0.0001f) { rightX /= rightLen; rightY /= rightLen; rightZ /= rightLen; }

        // Up vector = right x forward
        float upX = rightY * fwdZ - rightZ * fwdY;
        float upY = rightZ * fwdX - rightX * fwdZ;
        float upZ = rightX * fwdY - rightY * fwdX;

        // Normalize up
        float upLen = std::sqrt(upX*upX + upY*upY + upZ*upZ);
        if (upLen > 0.0001f) { upX /= upLen; upY /= upLen; upZ /= upLen; }

        // Calculate view plane size at target distance
        float tanHalfFov = std::tan(m_settings.fov * DEG_TO_RAD * 0.5f);
        float halfHeight = oldDistance * tanHalfFov;
        float halfWidth = halfHeight * m_aspectRatio;

        // World offset from view center to cursor point
        float worldOffsetX = rightX * ndcX * halfWidth + upX * ndcY * halfHeight;
        float worldOffsetY = rightY * ndcX * halfWidth + upY * ndcY * halfHeight;
        float worldOffsetZ = rightZ * ndcX * halfWidth + upZ * ndcY * halfHeight;

        // The cursor point in world space (relative to target)
        // To keep this point stationary, we need to move the target
        // New offset at new distance
        float newHalfHeight = newDistance * tanHalfFov;
        float newHalfWidth = newHalfHeight * m_aspectRatio;

        float newWorldOffsetX = rightX * ndcX * newHalfWidth + upX * ndcY * newHalfHeight;
        float newWorldOffsetY = rightY * ndcX * newHalfWidth + upY * ndcY * newHalfHeight;
        float newWorldOffsetZ = rightZ * ndcX * newHalfWidth + upZ * ndcY * newHalfHeight;

        // Move target to compensate for the offset change
        m_targetState.targetX += worldOffsetX - newWorldOffsetX;
        m_targetState.targetY += worldOffsetY - newWorldOffsetY;
        m_targetState.targetZ += worldOffsetZ - newWorldOffsetZ;
    }
    
    m_targetState.distance = newDistance;
    
    // Minimal velocity for smooth transition
    m_zoomVelocity = 0.0f;
}

// =============================================================================
// View Presets & Navigation
// =============================================================================

void OrbitCamera::setViewPreset(ViewPreset preset, bool animate)
{
    if (preset == ViewPreset::Custom)
        return;

    CameraState newState = m_targetState;
    getPresetAngles(preset, newState.yaw, newState.pitch);

    if (animate && m_settings.enableAnimations)
    {
        m_animStartState = m_currentState;
        m_animEndState = newState;
        m_animationDuration = m_settings.animationDuration;
        m_animationTime = 0.0f;
        m_isAnimating = true;

        // Stop inertia during animation
        m_yawVelocity = m_pitchVelocity = 0.0f;
        m_panVelocityX = m_panVelocityY = 0.0f;
    }
    else
    {
        m_targetState = newState;
        m_currentState = newState;
    }
}

ViewPreset OrbitCamera::getCurrentPreset() const
{
    const float epsilon = 1.0f; // Tolerance for angle matching

    float yaw = normalizeAngle(m_currentState.yaw);
    float pitch = m_currentState.pitch;

    // Check each preset
    if (std::abs(yaw - 0.0f) < epsilon && std::abs(pitch - 0.0f) < epsilon)
        return ViewPreset::Front;
    if (std::abs(yaw - 180.0f) < epsilon && std::abs(pitch - 0.0f) < epsilon)
        return ViewPreset::Back;
    if (std::abs(yaw - 90.0f) < epsilon && std::abs(pitch - 0.0f) < epsilon)
        return ViewPreset::Left;
    if ((std::abs(yaw - 270.0f) < epsilon || std::abs(yaw + 90.0f) < epsilon) && std::abs(pitch - 0.0f) < epsilon)
        return ViewPreset::Right;
    if (std::abs(pitch - 89.0f) < epsilon)
        return ViewPreset::Top;
    if (std::abs(pitch + 89.0f) < epsilon)
        return ViewPreset::Bottom;
    if (std::abs(yaw - 45.0f) < epsilon && std::abs(pitch - 35.264f) < epsilon)
        return ViewPreset::Isometric;

    return ViewPreset::Custom;
}

void OrbitCamera::reset(bool animate)
{
    CameraState defaultState{};

    if (animate && m_settings.enableAnimations)
    {
        m_animStartState = m_currentState;
        m_animEndState = defaultState;
        m_animationDuration = m_settings.animationDuration;
        m_animationTime = 0.0f;
        m_isAnimating = true;

        // Stop inertia
        m_yawVelocity = m_pitchVelocity = 0.0f;
        m_panVelocityX = m_panVelocityY = 0.0f;
        m_zoomVelocity = 0.0f;
    }
    else
    {
        m_targetState = defaultState;
        m_currentState = defaultState;
    }
}

void OrbitCamera::fitToModel(float minX, float minY, float minZ,
                              float maxX, float maxY, float maxZ,
                              bool animate)
{
    CameraState newState = m_targetState;

    // Center target on model
    newState.targetX = (minX + maxX) * 0.5f;
    newState.targetY = (minY + maxY) * 0.5f;
    newState.targetZ = (minZ + maxZ) * 0.5f;

    // Calculate distance to fit model in view
    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    float diagonal = std::sqrt(dx*dx + dy*dy + dz*dz);

    float fovRad = m_settings.fov * DEG_TO_RAD;
    newState.distance = (diagonal * 0.5f) / std::tan(fovRad * 0.5f) * 1.5f;
    newState.distance = std::clamp(newState.distance, m_settings.minDistance, m_settings.maxDistance);

    // Update ortho scale
    newState.orthoScale = diagonal * 0.6f;

    if (animate && m_settings.enableAnimations)
    {
        m_animStartState = m_currentState;
        m_animEndState = newState;
        m_animationDuration = m_settings.animationDuration;
        m_animationTime = 0.0f;
        m_isAnimating = true;

        // Stop inertia
        m_yawVelocity = m_pitchVelocity = 0.0f;
        m_panVelocityX = m_panVelocityY = 0.0f;
    }
    else
    {
        m_targetState = newState;
        m_currentState = newState;
    }
}

void OrbitCamera::focusOnPoint(float x, float y, float z, float distance, bool animate)
{
    CameraState newState = m_targetState;
    newState.targetX = x;
    newState.targetY = y;
    newState.targetZ = z;

    if (distance > 0.0f)
    {
        newState.distance = std::clamp(distance, m_settings.minDistance, m_settings.maxDistance);
    }

    if (animate && m_settings.enableAnimations)
    {
        m_animStartState = m_currentState;
        m_animEndState = newState;
        m_animationDuration = m_settings.animationDuration;
        m_animationTime = 0.0f;
        m_isAnimating = true;

        // Stop inertia
        m_yawVelocity = m_pitchVelocity = 0.0f;
        m_panVelocityX = m_panVelocityY = 0.0f;
    }
    else
    {
        m_targetState = newState;
        m_currentState = newState;
    }
}

// =============================================================================
// Projection Mode
// =============================================================================

void OrbitCamera::toggleOrthographic()
{
    m_targetState.orthographic = !m_targetState.orthographic;
}

// =============================================================================
// State Management
// =============================================================================

void OrbitCamera::setTarget(float x, float y, float z)
{
    m_targetState.targetX = x;
    m_targetState.targetY = y;
    m_targetState.targetZ = z;
}

void OrbitCamera::setState(const CameraState& state)
{
    m_currentState = state;
    m_targetState = state;
    m_isAnimating = false;
    m_yawVelocity = m_pitchVelocity = 0.0f;
    m_panVelocityX = m_panVelocityY = 0.0f;
    m_zoomVelocity = 0.0f;
}

void OrbitCamera::getTarget(float& x, float& y, float& z) const
{
    x = m_currentState.targetX;
    y = m_currentState.targetY;
    z = m_currentState.targetZ;
}

void OrbitCamera::getPosition(float& x, float& y, float& z) const
{
    float yawRad = m_currentState.yaw * DEG_TO_RAD;
    float pitchRad = m_currentState.pitch * DEG_TO_RAD;

    // Z-up coordinate system: yaw rotates around Z axis
    float cosPitch = std::cos(pitchRad);
    x = m_currentState.targetX + m_currentState.distance * cosPitch * std::sin(yawRad);
    y = m_currentState.targetY + m_currentState.distance * cosPitch * std::cos(yawRad);
    z = m_currentState.targetZ + m_currentState.distance * std::sin(pitchRad);
}

bool OrbitCamera::hasInertia() const
{
    return std::abs(m_yawVelocity) > m_settings.minVelocity ||
           std::abs(m_pitchVelocity) > m_settings.minVelocity ||
           std::abs(m_panVelocityX) > m_settings.minVelocity ||
           std::abs(m_panVelocityY) > m_settings.minVelocity ||
           std::abs(m_zoomVelocity) > m_settings.minVelocity;
}

// =============================================================================
// Constraint Helpers
// =============================================================================

float OrbitCamera::applyYawConstraint(float yaw) const
{
    return normalizeAngle(yaw);
}

float OrbitCamera::applyPitchConstraint(float pitch) const
{
    return std::clamp(pitch, m_settings.minPitch, m_settings.maxPitch);
}

float OrbitCamera::snapAngle(float angle, float snapTo) const
{
    return std::round(angle / snapTo) * snapTo;
}

// =============================================================================
// Matrix Generation
// =============================================================================

void OrbitCamera::getViewMatrix(float* m) const
{
    float posX, posY, posZ;
    getPosition(posX, posY, posZ);

    // Look direction
    float lookX = m_currentState.targetX - posX;
    float lookY = m_currentState.targetY - posY;
    float lookZ = m_currentState.targetZ - posZ;

    // Normalize
    float len = std::sqrt(lookX*lookX + lookY*lookY + lookZ*lookZ);
    if (len > 0.0001f)
    {
        lookX /= len; lookY /= len; lookZ /= len;
    }

    // Up vector (world Z for Z-up coordinate system)
    float upX = 0.0f, upY = 0.0f, upZ = 1.0f;

    // Right = look x up
    float rightX = lookY * upZ - lookZ * upY;
    float rightY = lookZ * upX - lookX * upZ;
    float rightZ = lookX * upY - lookY * upX;

    // Normalize right
    len = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
    if (len > 0.0001f)
    {
        rightX /= len; rightY /= len; rightZ /= len;
    }

    // Recalculate up = right x look
    upX = rightY * lookZ - rightZ * lookY;
    upY = rightZ * lookX - rightX * lookZ;
    upZ = rightX * lookY - rightY * lookX;

    // Build view matrix (row-major for HLSL row-vector * matrix multiplication)
    // For mul(v, M) form, basis vectors go in COLUMNS, not rows
    m[0] = rightX;  m[1] = upX;     m[2] = lookX;    m[3] = 0.0f;
    m[4] = rightY;  m[5] = upY;     m[6] = lookY;    m[7] = 0.0f;
    m[8] = rightZ;  m[9] = upZ;     m[10] = lookZ;   m[11] = 0.0f;
    m[12] = -(rightX*posX + rightY*posY + rightZ*posZ);
    m[13] = -(upX*posX + upY*posY + upZ*posZ);
    m[14] = -(lookX*posX + lookY*posY + lookZ*posZ);
    m[15] = 1.0f;
}

void OrbitCamera::getProjectionMatrix(float* m) const
{
    std::memset(m, 0, 16 * sizeof(float));

    if (m_currentState.orthographic)
    {
        // Orthographic projection
        float halfWidth = m_currentState.orthoScale * m_aspectRatio;
        float halfHeight = m_currentState.orthoScale;
        float zNear = m_settings.nearPlane;
        float zFar = m_settings.farPlane;

        m[0] = 1.0f / halfWidth;
        m[5] = 1.0f / halfHeight;
        m[10] = 1.0f / (zFar - zNear);
        m[14] = -zNear / (zFar - zNear);
        m[15] = 1.0f;
    }
    else
    {
        // Perspective projection
        float fovRad = m_settings.fov * DEG_TO_RAD;
        float tanHalfFov = std::tan(fovRad * 0.5f);

        float yScale = 1.0f / tanHalfFov;
        float xScale = yScale / m_aspectRatio;
        float zNear = m_settings.nearPlane;
        float zFar = m_settings.farPlane;

        m[0] = xScale;
        m[5] = yScale;
        m[10] = zFar / (zFar - zNear);
        m[11] = 1.0f;
        m[14] = -zNear * zFar / (zFar - zNear);
    }
}

void OrbitCamera::screenToWorldRay(float screenX, float screenY,
                                    float& outDirX, float& outDirY, float& outDirZ) const
{
    // Convert screen coords to normalized device coords (-1 to 1)
    float ndcX = (screenX * 2.0f - 1.0f);
    float ndcY = (1.0f - screenY * 2.0f);

    // Get camera position and orientation
    float posX, posY, posZ;
    getPosition(posX, posY, posZ);

    float yawRad = m_currentState.yaw * DEG_TO_RAD;
    float pitchRad = m_currentState.pitch * DEG_TO_RAD;

    // Calculate camera basis vectors (Z-up system)
    float cosPitch = std::cos(pitchRad);
    float sinPitch = std::sin(pitchRad);
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);

    // Forward (look direction) - Z-up system
    float fwdX = -cosPitch * sinYaw;
    float fwdY = -cosPitch * cosYaw;
    float fwdZ = -sinPitch;

    // World up is Z
    float worldUpX = 0.0f, worldUpY = 0.0f, worldUpZ = 1.0f;

    // Right = forward x worldUp
    float rightX = fwdY * worldUpZ - fwdZ * worldUpY;
    float rightY = fwdZ * worldUpX - fwdX * worldUpZ;
    float rightZ = fwdX * worldUpY - fwdY * worldUpX;

    // Normalize right
    float rightLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
    if (rightLen > 0.0001f)
    {
        rightX /= rightLen; rightY /= rightLen; rightZ /= rightLen;
    }

    // Up = right x forward
    float upX = rightY * fwdZ - rightZ * fwdY;
    float upY = rightZ * fwdX - rightX * fwdZ;
    float upZ = rightX * fwdY - rightY * fwdX;

    // Calculate ray direction based on FOV
    float fovRad = m_settings.fov * DEG_TO_RAD;
    float tanHalfFov = std::tan(fovRad * 0.5f);

    float rayX = fwdX + rightX * ndcX * tanHalfFov * m_aspectRatio + upX * ndcY * tanHalfFov;
    float rayY = fwdY + rightY * ndcX * tanHalfFov * m_aspectRatio + upY * ndcY * tanHalfFov;
    float rayZ = fwdZ + rightZ * ndcX * tanHalfFov * m_aspectRatio + upZ * ndcY * tanHalfFov;

    // Normalize
    float len = std::sqrt(rayX*rayX + rayY*rayY + rayZ*rayZ);
    if (len > 0.0001f)
    {
        outDirX = rayX / len;
        outDirY = rayY / len;
        outDirZ = rayZ / len;
    }
    else
    {
        outDirX = fwdX;
        outDirY = fwdY;
        outDirZ = fwdZ;
    }
}

bool OrbitCamera::worldToScreen(float worldX, float worldY, float worldZ,
                                 float& outScreenX, float& outScreenY) const
{
    // Get view and projection matrices
    float viewMatrix[16];
    float projMatrix[16];
    getViewMatrix(viewMatrix);
    getProjectionMatrix(projMatrix);

    // Transform world position by view matrix (row-major)
    // Result is in view/camera space
    float viewX = viewMatrix[0] * worldX + viewMatrix[4] * worldY + viewMatrix[8] * worldZ + viewMatrix[12];
    float viewY = viewMatrix[1] * worldX + viewMatrix[5] * worldY + viewMatrix[9] * worldZ + viewMatrix[13];
    float viewZ = viewMatrix[2] * worldX + viewMatrix[6] * worldY + viewMatrix[10] * worldZ + viewMatrix[14];
    float viewW = viewMatrix[3] * worldX + viewMatrix[7] * worldY + viewMatrix[11] * worldZ + viewMatrix[15];

    // Transform by projection matrix
    float clipX = projMatrix[0] * viewX + projMatrix[4] * viewY + projMatrix[8] * viewZ + projMatrix[12] * viewW;
    float clipY = projMatrix[1] * viewX + projMatrix[5] * viewY + projMatrix[9] * viewZ + projMatrix[13] * viewW;
    // clipZ not needed for 2D screen projection
    float clipW = projMatrix[3] * viewX + projMatrix[7] * viewY + projMatrix[11] * viewZ + projMatrix[15] * viewW;

    // Check if point is behind camera
    if (clipW <= 0.0f)
    {
        outScreenX = 0.0f;
        outScreenY = 0.0f;
        return false;
    }

    // Perspective divide to get NDC (-1 to 1)
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    // Convert to screen coordinates (0 to 1)
    outScreenX = (ndcX + 1.0f) * 0.5f;
    outScreenY = (1.0f - ndcY) * 0.5f;  // Flip Y for screen space

    return true;
}

} // namespace MoldWing
