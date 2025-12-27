/*
 *  MoldWing - Orbit Camera (Enhanced)
 *  Professional camera system with smoothing, inertia, and view presets
 */

#pragma once

#include "CameraSettings.hpp"
#include <cmath>
#include <chrono>

namespace MoldWing
{

/**
 * @brief Enhanced orbit camera with professional features
 *
 * Features:
 * - Smooth interpolation for silky movement
 * - Inertia system for natural feel
 * - Zoom to cursor position
 * - View presets with animated transitions
 * - Rotation constraints and angle snapping
 * - Orthographic/Perspective toggle
 */
class OrbitCamera
{
public:
    OrbitCamera();

    // ==========================================================================
    // Core Update (call every frame)
    // ==========================================================================

    /**
     * @brief Update camera interpolation and inertia
     * @param deltaTime Time since last frame in seconds
     */
    void update(float deltaTime);

    // ==========================================================================
    // Input Handling
    // ==========================================================================

    /**
     * @brief Begin rotation drag
     */
    void beginRotate();

    /**
     * @brief End rotation drag (applies inertia)
     */
    void endRotate();

    /**
     * @brief Rotate camera with optional constraint
     * @param deltaYaw Horizontal rotation in pixels
     * @param deltaPitch Vertical rotation in pixels
     * @param constraint Rotation constraint mode
     */
    void rotate(float deltaYaw, float deltaPitch,
                RotationConstraint constraint = RotationConstraint::None);

    /**
     * @brief Begin pan drag
     */
    void beginPan();

    /**
     * @brief End pan drag (applies inertia)
     */
    void endPan();

    /**
     * @brief Pan camera with 1:1 screen-to-world mapping
     * @param pixelDeltaX Horizontal movement in pixels
     * @param pixelDeltaY Vertical movement in pixels
     * @param viewportWidth Viewport width in pixels
     * @param viewportHeight Viewport height in pixels
     */
    void pan(float pixelDeltaX, float pixelDeltaY, int viewportWidth, int viewportHeight);

    /**
     * @brief Zoom camera
     * @param delta Zoom amount (positive = zoom in)
     * @param cursorX Optional cursor X position (0-1 normalized)
     * @param cursorY Optional cursor Y position (0-1 normalized)
     */
    void zoom(float delta, float cursorX = 0.5f, float cursorY = 0.5f);

    // ==========================================================================
    // View Presets & Navigation
    // ==========================================================================

    /**
     * @brief Set view to a preset with optional animation
     * @param preset The view preset to apply
     * @param animate Whether to animate the transition
     */
    void setViewPreset(ViewPreset preset, bool animate = true);

    /**
     * @brief Get current view preset (Custom if not matching any preset)
     */
    ViewPreset getCurrentPreset() const;

    /**
     * @brief Reset camera to default view
     * @param animate Whether to animate the transition
     */
    void reset(bool animate = true);

    /**
     * @brief Fit camera to model bounds
     * @param minX/Y/Z Minimum bounds
     * @param maxX/Y/Z Maximum bounds
     * @param animate Whether to animate the transition
     */
    void fitToModel(float minX, float minY, float minZ,
                    float maxX, float maxY, float maxZ,
                    bool animate = true);

    /**
     * @brief Focus on a point in world space
     * @param x/y/z World position to focus on
     * @param distance Optional distance (-1 = keep current)
     * @param animate Whether to animate the transition
     */
    void focusOnPoint(float x, float y, float z,
                      float distance = -1.0f, bool animate = true);

    // ==========================================================================
    // Projection Mode
    // ==========================================================================

    /**
     * @brief Toggle between orthographic and perspective projection
     */
    void toggleOrthographic();

    /**
     * @brief Set orthographic mode
     */
    void setOrthographic(bool ortho) { m_targetState.orthographic = ortho; }

    /**
     * @brief Check if in orthographic mode
     */
    bool isOrthographic() const { return m_currentState.orthographic; }

    // ==========================================================================
    // Settings
    // ==========================================================================

    /**
     * @brief Get camera settings for modification
     */
    CameraSettings& settings() { return m_settings; }
    const CameraSettings& settings() const { return m_settings; }

    /**
     * @brief Set aspect ratio
     */
    void setAspectRatio(float aspect) { m_aspectRatio = aspect; }

    /**
     * @brief Set target position directly
     */
    void setTarget(float x, float y, float z);

    // ==========================================================================
    // State Getters
    // ==========================================================================

    /**
     * @brief Get current camera state
     */
    CameraState getState() const { return m_currentState; }

    /**
     * @brief Set camera state directly (no animation)
     */
    void setState(const CameraState& state);

    /**
     * @brief Get current yaw angle (degrees)
     */
    float yaw() const { return m_currentState.yaw; }

    /**
     * @brief Get current pitch angle (degrees)
     */
    float pitch() const { return m_currentState.pitch; }

    /**
     * @brief Get current distance from target
     */
    float distance() const { return m_currentState.distance; }

    /**
     * @brief Get current target position
     */
    void getTarget(float& x, float& y, float& z) const;

    /**
     * @brief Get camera position in world space
     */
    void getPosition(float& x, float& y, float& z) const;

    /**
     * @brief Check if camera is currently animating
     */
    bool isAnimating() const { return m_isAnimating; }

    /**
     * @brief Check if camera has inertia velocity
     */
    bool hasInertia() const;

    // ==========================================================================
    // Matrix Generation
    // ==========================================================================

    /**
     * @brief Get view matrix (row-major for HLSL)
     */
    void getViewMatrix(float* outMatrix) const;

    /**
     * @brief Get projection matrix (row-major for HLSL)
     */
    void getProjectionMatrix(float* outMatrix) const;

    /**
     * @brief Unproject screen position to world ray direction
     * @param screenX Screen X (0-1 normalized)
     * @param screenY Screen Y (0-1 normalized)
     * @param outDirX/Y/Z Output ray direction
     */
    void screenToWorldRay(float screenX, float screenY,
                          float& outDirX, float& outDirY, float& outDirZ) const;

private:
    // Interpolation helpers
    void updateSmoothing(float deltaTime);
    void updateInertia(float deltaTime);
    void updateAnimation(float deltaTime);

    // Apply angle constraints
    float applyYawConstraint(float yaw) const;
    float applyPitchConstraint(float pitch) const;
    float snapAngle(float angle, float snapTo) const;

    // State
    CameraState m_currentState;     // Current interpolated state
    CameraState m_targetState;      // Target state (input destination)

    // Animation
    bool m_isAnimating = false;
    float m_animationTime = 0.0f;
    float m_animationDuration = 0.3f;
    CameraState m_animStartState;
    CameraState m_animEndState;

    // Inertia velocities
    float m_yawVelocity = 0.0f;
    float m_pitchVelocity = 0.0f;
    float m_panVelocityX = 0.0f;
    float m_panVelocityY = 0.0f;
    float m_zoomVelocity = 0.0f;

    // Drag state
    bool m_isDraggingRotate = false;
    bool m_isDraggingPan = false;
    float m_lastDeltaYaw = 0.0f;
    float m_lastDeltaPitch = 0.0f;
    float m_lastDeltaPanX = 0.0f;
    float m_lastDeltaPanY = 0.0f;

    // Settings
    CameraSettings m_settings;
    float m_aspectRatio = 16.0f / 9.0f;

    // Time tracking
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    bool m_firstUpdate = true;
};

} // namespace MoldWing
