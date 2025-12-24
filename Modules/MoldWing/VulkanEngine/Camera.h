#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace MoldWing {

/**
 * @brief Camera manipulator based on OpenSceneGraph's TrackballManipulator
 *
 * Features:
 * - Trackball/Arcball rotation around center point
 * - Pan along view plane
 * - Dolly (zoom) preserving look-at center
 * - Smooth and intuitive controls similar to OSG
 */
class Camera {
public:
    /**
     * @brief Construct a new Camera object
     * @param eye Initial camera position
     * @param center Look-at center point (rotation pivot)
     * @param up Up vector (default: Y-up)
     */
    Camera(const glm::vec3& eye = glm::vec3(3.0f, 3.0f, 3.0f),
           const glm::vec3& center = glm::vec3(0.0f, 0.0f, 0.0f),
           const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f))
        : eye_(eye)
        , center_(center)
        , up_(up)
        , rotation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
        , rotationSensitivity_(1.0f)
        , panSensitivity_(1.0f)
        , zoomSensitivity_(1.0f)
        , minimumDistance_(0.1f)
    {
        updateRotationFromVectors();
    }

    /**
     * @brief Get the view matrix
     */
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(eye_, center_, up_);
    }

    /**
     * @brief Get camera position (eye)
     */
    glm::vec3 getPosition() const { return eye_; }

    /**
     * @brief Get center position (look-at point)
     */
    glm::vec3 getCenter() const { return center_; }

    /**
     * @brief Get up vector
     */
    glm::vec3 getUp() const { return up_; }

    /**
     * @brief Get distance from eye to center
     */
    float getDistance() const {
        return glm::length(eye_ - center_);
    }

    /**
     * @brief Process trackball rotation (left button drag)
     *
     * Implements virtual trackball rotation:
     * - Maps 2D mouse movement to 3D rotation on a virtual sphere
     * - Rotates camera around the center point
     *
     * @param dx Mouse X offset normalized to [-1, 1] or in pixels
     * @param dy Mouse Y offset normalized to [-1, 1] or in pixels
     * @param screenWidth Width of viewport (for normalization)
     * @param screenHeight Height of viewport (for normalization)
     */
    void rotate(float dx, float dy, float screenWidth, float screenHeight) {
        if (dx == 0.0f && dy == 0.0f) return;

        // Normalize mouse movement to [-1, 1] range
        float ndx = dx / screenWidth * 2.0f;
        float ndy = -dy / screenHeight * 2.0f; // Negate dy to fix up/down direction

        // Get current viewing vectors
        glm::vec3 lookDir = glm::normalize(center_ - eye_);
        glm::vec3 right = glm::normalize(glm::cross(lookDir, up_));
        glm::vec3 trueUp = glm::normalize(glm::cross(right, lookDir));

        // Calculate rotation axes in world space
        glm::vec3 rotateAxis = right * ndy - trueUp * ndx;
        float angle = glm::length(rotateAxis) * rotationSensitivity_;

        if (angle > 0.0f) {
            rotateAxis = glm::normalize(rotateAxis);

            // Create rotation quaternion
            glm::quat deltaRotation = glm::angleAxis(angle, rotateAxis);

            // Calculate vector from center to eye
            glm::vec3 centerToEye = eye_ - center_;

            // Apply rotation to the eye position around center
            centerToEye = deltaRotation * centerToEye;
            eye_ = center_ + centerToEye;

            // Rotate up vector as well
            up_ = deltaRotation * up_;
            up_ = glm::normalize(up_);
        }
    }

    /**
     * @brief Process pan (middle button drag)
     *
     * Pans camera and center along the view plane:
     * - Movement is in screen space (right/up directions)
     * - Both eye and center move together maintaining view direction
     * - Pan speed scales with distance from center
     *
     * @param dx Mouse X offset in pixels
     * @param dy Mouse Y offset in pixels
     * @param screenWidth Width of viewport
     * @param screenHeight Height of viewport
     */
    void pan(float dx, float dy, float screenWidth, float screenHeight) {
        if (dx == 0.0f && dy == 0.0f) return;

        // Get viewing vectors
        glm::vec3 lookDir = glm::normalize(center_ - eye_);
        glm::vec3 right = glm::normalize(glm::cross(lookDir, up_));
        glm::vec3 trueUp = glm::normalize(glm::cross(right, lookDir));

        // Scale pan speed based on distance to center
        float distance = getDistance();
        float scaleFactor = distance * panSensitivity_ * 0.002f;

        // Calculate pan offset in world space
        // Move camera opposite to mouse movement to make object follow mouse
        glm::vec3 offset = (right * -dx + trueUp * dy) * scaleFactor;

        // Move both eye and center (maintain relative position)
        eye_ += offset;
        center_ += offset;
    }

    /**
     * @brief Process zoom/dolly (scroll wheel)
     *
     * Moves camera closer/further from center:
     * - Positive delta moves closer (zoom in)
     * - Negative delta moves further (zoom out)
     * - Preserves look direction and center point
     * - Enforces minimum distance
     *
     * @param delta Scroll wheel delta (positive = zoom in)
     */
    void zoom(float delta) {
        if (delta == 0.0f) return;

        glm::vec3 lookDir = glm::normalize(center_ - eye_);
        float distance = getDistance();

        // Calculate zoom amount (percentage of current distance)
        float zoomAmount = distance * delta * zoomSensitivity_ * 0.1f;

        // Apply zoom
        glm::vec3 newEye = eye_ + lookDir * zoomAmount;
        float newDistance = glm::length(newEye - center_);

        // Enforce minimum distance
        if (newDistance > minimumDistance_) {
            eye_ = newEye;
        } else {
            // Clamp to minimum distance
            eye_ = center_ - lookDir * minimumDistance_;
        }
    }

    /**
     * @brief Set rotation sensitivity (default: 1.0)
     */
    void setRotationSensitivity(float sensitivity) {
        rotationSensitivity_ = sensitivity;
    }

    /**
     * @brief Set pan sensitivity (default: 1.0)
     */
    void setPanSensitivity(float sensitivity) {
        panSensitivity_ = sensitivity;
    }

    /**
     * @brief Set zoom sensitivity (default: 1.0)
     */
    void setZoomSensitivity(float sensitivity) {
        zoomSensitivity_ = sensitivity;
    }

    /**
     * @brief Set minimum distance from center (default: 0.1)
     */
    void setMinimumDistance(float distance) {
        minimumDistance_ = distance;
    }

    /**
     * @brief Set camera position directly
     */
    void setTransformation(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        eye_ = eye;
        center_ = center;
        up_ = glm::normalize(up);
        updateRotationFromVectors();
    }

    /**
     * @brief Reset to home position
     */
    void home() {
        eye_ = glm::vec3(3.0f, 3.0f, 3.0f);
        center_ = glm::vec3(0.0f, 0.0f, 0.0f);
        up_ = glm::vec3(0.0f, 1.0f, 0.0f);
        updateRotationFromVectors();
    }

private:
    /**
     * @brief Update rotation quaternion from current eye/center/up vectors
     */
    void updateRotationFromVectors() {
        glm::vec3 lookDir = glm::normalize(center_ - eye_);
        glm::vec3 right = glm::normalize(glm::cross(lookDir, up_));
        glm::vec3 trueUp = glm::cross(right, lookDir);

        glm::mat3 rotationMatrix;
        rotationMatrix[0] = right;
        rotationMatrix[1] = trueUp;
        rotationMatrix[2] = -lookDir;

        rotation_ = glm::quat_cast(rotationMatrix);
    }

    // Camera state (similar to OSG's eye/center/up)
    glm::vec3 eye_;      // Camera position
    glm::vec3 center_;   // Look-at center (rotation pivot)
    glm::vec3 up_;       // Up vector

    // Rotation state
    glm::quat rotation_;

    // Interaction parameters
    float rotationSensitivity_;
    float panSensitivity_;
    float zoomSensitivity_;
    float minimumDistance_;
};

} // namespace MoldWing
