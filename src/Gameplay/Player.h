#pragma once

#include "World/WorldConstants.h"

#include <glm/glm.hpp>

struct GLFWwindow;
class VoxelWorld;

// The player's flying drone: camera position and direction, mouse look, physics-based movement
// and collision with solid voxels. Positions are in world units.
class Player {
public:
    // Drone boundary radius (slightly smaller than 0.5 voxels for fitting in 1-voxel gaps)
    static constexpr float RADIUS = 0.45f / VOXELS_PER_UNIT;

    void SetPosition(glm::vec3 position) { m_Position = position; }

    // Mouse look; ignored (and the next move re-anchored) while the cursor is not captured
    void OnMouseMove(double x, double y, bool cursorCaptured);

    // WASD / Space / Ctrl / Shift flight with sliding collision
    void UpdateMovement(GLFWwindow* window, float deltaTime, const VoxelWorld& world);

    // Holds the camera at a voxel position and direction (regression screenshots)
    void LockView(glm::vec3 voxelPosition, float yaw, float pitch);

    bool OverlapsVoxel(glm::ivec3 voxel) const;

    glm::vec3 Position() const { return m_Position; }
    glm::vec3 Front() const { return m_Front; }
    glm::vec3 Up() const { return m_Up; }
    glm::ivec3 ChunkCoord() const { return glm::ivec3(glm::floor(m_Position * VOXELS_PER_UNIT / (float)CHUNK_SIZE)); }

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
