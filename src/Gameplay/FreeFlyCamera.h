#pragma once

#include "Gameplay/Camera.h"
#include "World/WorldConstants.h"

#include <glm/glm.hpp>

struct GLFWwindow;
class VoxelWorld;

// Debug free-fly camera (F1): a flying drone with mouse look, physics-based movement and
// collision with solid voxels. Positions are in world units.
class FreeFlyCamera : public ICamera {
public:
    // Drone boundary radius (slightly smaller than 0.5 voxels for fitting in 1-voxel gaps)
    static constexpr float RADIUS = 0.45f / VOXELS_PER_UNIT;

    void SetPosition(glm::vec3 position) { m_Position = position; }

    // Mouse look; ignored (and the next move re-anchored) while the cursor is not captured
    void OnMouseMove(double x, double y, bool cursorCaptured);

    // WASD / Space / Ctrl / Shift flight with sliding collision
    void UpdateMovement(GLFWwindow* window, float deltaTime, const VoxelWorld& world);

    // Look direction in degrees (pitch > 0 looks up)
    void SetLook(float yaw, float pitch);
    float Yaw() const { return m_Yaw; }
    float Pitch() const { return m_Pitch; }

    // Holds the camera at a voxel position and direction (regression screenshots)
    void LockView(glm::vec3 voxelPosition, float yaw, float pitch);

    bool OverlapsVoxel(glm::ivec3 voxel) const;

    glm::vec3 Position() const override { return m_Position; }
    glm::vec3 Front() const override { return m_Front; }
    glm::vec3 Up() const override { return m_Up; }
    glm::vec3 FocusPoint() const override { return m_Position; }

private:
    bool IsColliding(glm::vec3 position, const VoxelWorld& world) const;
    void ResolveCollisions(const VoxelWorld& world);
    void UpdateFront();

    glm::vec3 m_Position = glm::vec3(0.5f, 0.09f, 0.5f);
    glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_Velocity = glm::vec3(0.0f);
    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;
    bool m_FirstMouse = true;
    float m_LastMouseX = 960.0f;
    float m_LastMouseY = 540.0f;
};
