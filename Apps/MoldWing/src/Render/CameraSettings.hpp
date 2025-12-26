/*
 *  MoldWing - Camera Settings
 *  Configuration structure for camera behavior
 */

#pragma once

namespace MoldWing
{

/**
 * @brief View preset enumeration for standard views
 */
enum class ViewPreset
{
    Custom,     // Current user-defined view
    Front,      // +Z looking towards -Z (yaw=0, pitch=0)
    Back,       // -Z looking towards +Z (yaw=180, pitch=0)
    Left,       // -X looking towards +X (yaw=90, pitch=0)
    Right,      // +X looking towards -X (yaw=-90, pitch=0)
    Top,        // +Y looking down (yaw=0, pitch=89)
    Bottom,     // -Y looking up (yaw=0, pitch=-89)
    Isometric   // 45 degree isometric view (yaw=45, pitch=35.264)
};

/**
 * @brief Rotation constraint mode
 */
enum class RotationConstraint
{
    None,           // Free rotation
    HorizontalOnly, // Only horizontal (yaw) rotation (Shift held)
    VerticalOnly,   // Only vertical (pitch) rotation (Shift held)
    Snap45,         // Snap to 45 degree increments (Ctrl held)
    Snap90          // Snap to 90 degree increments
};

/**
 * @brief Mouse interaction mode (Blender-style by default)
 */
enum class InteractionStyle
{
    Blender,    // MMB=rotate, Shift+MMB=pan, Scroll=zoom
    Maya,       // Alt+LMB=rotate, Alt+MMB=pan, Alt+RMB=zoom
    Current     // LMB=rotate, MMB/RMB=pan, Scroll=zoom
};

/**
 * @brief Camera settings structure
 */
struct CameraSettings
{
    // Sensitivity settings
    float rotationSensitivity = 0.3f;   // Degrees per pixel
    float panSensitivity = 1.0f;        // Pan speed multiplier
    float zoomSensitivity = 0.1f;       // Zoom speed multiplier

    // Smoothing settings
    float smoothSpeed = 12.0f;          // Interpolation speed (higher = faster response)
    bool enableSmoothing = true;        // Enable smooth camera movement

    // Inertia settings
    float inertiaDamping = 0.88f;       // Velocity damping per frame (0-1, lower = more damping)
    float minVelocity = 0.001f;         // Minimum velocity before stopping
    bool enableInertia = true;          // Enable inertia after mouse release

    // Interaction settings
    InteractionStyle interactionStyle = InteractionStyle::Blender;
    bool invertY = false;               // Invert Y axis for rotation
    bool zoomToCursor = true;           // Zoom towards cursor position

    // Animation settings
    float animationDuration = 0.3f;     // Duration for view transitions (seconds)
    bool enableAnimations = true;       // Enable animated view transitions

    // Constraints
    float snapAngle = 45.0f;            // Angle for snap rotation (degrees)

    // Projection settings
    float fov = 45.0f;                  // Field of view (degrees)
    float nearPlane = 0.01f;            // Near clipping plane
    float farPlane = 10000.0f;          // Far clipping plane

    // Distance limits
    float minDistance = 0.01f;          // Minimum orbit distance
    float maxDistance = 10000.0f;       // Maximum orbit distance

    // Pitch limits
    float minPitch = -89.0f;            // Minimum pitch angle
    float maxPitch = 89.0f;             // Maximum pitch angle
};

/**
 * @brief Camera state for animation interpolation
 */
struct CameraState
{
    float yaw = 45.0f;
    float pitch = 30.0f;
    float distance = 5.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    bool orthographic = false;
    float orthoScale = 1.0f;

    // Linear interpolation between states
    static CameraState lerp(const CameraState& a, const CameraState& b, float t)
    {
        CameraState result;
        result.yaw = a.yaw + (b.yaw - a.yaw) * t;
        result.pitch = a.pitch + (b.pitch - a.pitch) * t;
        result.distance = a.distance + (b.distance - a.distance) * t;
        result.targetX = a.targetX + (b.targetX - a.targetX) * t;
        result.targetY = a.targetY + (b.targetY - a.targetY) * t;
        result.targetZ = a.targetZ + (b.targetZ - a.targetZ) * t;
        result.orthographic = t > 0.5f ? b.orthographic : a.orthographic;
        result.orthoScale = a.orthoScale + (b.orthoScale - a.orthoScale) * t;
        return result;
    }
};

/**
 * @brief Get yaw/pitch for a view preset
 */
inline void getPresetAngles(ViewPreset preset, float& yaw, float& pitch)
{
    switch (preset)
    {
        case ViewPreset::Front:
            yaw = 0.0f; pitch = 0.0f;
            break;
        case ViewPreset::Back:
            yaw = 180.0f; pitch = 0.0f;
            break;
        case ViewPreset::Left:
            yaw = 90.0f; pitch = 0.0f;
            break;
        case ViewPreset::Right:
            yaw = -90.0f; pitch = 0.0f;
            break;
        case ViewPreset::Top:
            yaw = 0.0f; pitch = 89.0f;
            break;
        case ViewPreset::Bottom:
            yaw = 0.0f; pitch = -89.0f;
            break;
        case ViewPreset::Isometric:
            yaw = 45.0f; pitch = 35.264f;  // arctan(1/sqrt(2))
            break;
        default:
            // Keep current
            break;
    }
}

/**
 * @brief Get display name for view preset
 */
inline const char* getPresetName(ViewPreset preset)
{
    switch (preset)
    {
        case ViewPreset::Front:     return "Front";
        case ViewPreset::Back:      return "Back";
        case ViewPreset::Left:      return "Left";
        case ViewPreset::Right:     return "Right";
        case ViewPreset::Top:       return "Top";
        case ViewPreset::Bottom:    return "Bottom";
        case ViewPreset::Isometric: return "Isometric";
        default:                    return "Custom";
    }
}

} // namespace MoldWing
