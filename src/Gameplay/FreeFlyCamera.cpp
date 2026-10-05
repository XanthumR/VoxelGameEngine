#include "Gameplay/FreeFlyCamera.h"

#include "World/BlockTypes.h"
#include "World/VoxelWorld.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

void FreeFlyCamera::UpdateFront() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    m_Front = glm::normalize(front);
}

void FreeFlyCamera::OnMouseMove(double x, double y, bool cursorCaptured) {
    if (!cursorCaptured) {
        m_FirstMouse = true;
        return;
    }
    float xpos = static_cast<float>(x);
    float ypos = static_cast<float>(y);
    if (m_FirstMouse) {
        m_LastMouseX = xpos;
        m_LastMouseY = ypos;
        m_FirstMouse = false;
    }

    const float SENSITIVITY = 0.1f;
    float xoffset = (xpos - m_LastMouseX) * SENSITIVITY;
    float yoffset = (m_LastMouseY - ypos) * SENSITIVITY; // Reversed: screen y goes down
    m_LastMouseX = xpos;
    m_LastMouseY = ypos;

    m_Yaw += xoffset;
    m_Pitch = std::clamp(m_Pitch + yoffset, -89.0f, 89.0f);
    UpdateFront();
}

void FreeFlyCamera::SetLook(float yaw, float pitch) {
    m_Yaw = yaw;
    m_Pitch = pitch;
    UpdateFront();
}

void FreeFlyCamera::LockView(glm::vec3 voxelPosition, float yaw, float pitch) {
    m_Position = voxelPosition / VOXELS_PER_UNIT;
    m_Velocity = glm::vec3(0.0f);
    SetLook(yaw, pitch);
}

bool FreeFlyCamera::OverlapsVoxel(glm::ivec3 voxel) const {
    glm::vec3 minP = m_Position - glm::vec3(RADIUS);
    glm::vec3 maxP = m_Position + glm::vec3(RADIUS);
    glm::vec3 vMin = glm::vec3(voxel) / VOXELS_PER_UNIT;
    glm::vec3 vMax = glm::vec3(voxel + glm::ivec3(1)) / VOXELS_PER_UNIT;
    return glm::all(glm::lessThan(minP, vMax)) && glm::all(glm::greaterThan(maxP, vMin));
}

bool FreeFlyCamera::IsColliding(glm::vec3 position, const VoxelWorld& world) const {
    // The world is unbounded horizontally; only keep the player inside the vertical slab
    if (position.y < 0.0f || position.y > (float)WORLD_HEIGHT / VOXELS_PER_UNIT) return true;

    // Corners of the bounding box
    for (int ix = -1; ix <= 1; ix += 2) {
        for (int iy = -1; iy <= 1; iy += 2) {
            for (int iz = -1; iz <= 1; iz += 2) {
                glm::vec3 p = position + glm::vec3(ix * RADIUS, iy * RADIUS, iz * RADIUS);
                glm::ivec3 voxel = glm::ivec3(glm::floor(p * VOXELS_PER_UNIT));
                if (IsSolidBlock(world.GetVoxel(voxel.x, voxel.y, voxel.z))) return true;
            }
        }
    }
    return false;
}

// Pushes the player out of any voxel it overlaps, along the axis of least penetration
void FreeFlyCamera::ResolveCollisions(const VoxelWorld& world) {
    glm::vec3& pos = m_Position;
    glm::vec3& velocity = m_Velocity;
    const float r = RADIUS;

    // Up to 4 iterations to handle multi-surface/corner collisions
    for (int iter = 0; iter < 4; iter++) {
        glm::vec3 minP = pos - glm::vec3(r);
        glm::vec3 maxP = pos + glm::vec3(r);
        glm::ivec3 minVox = glm::ivec3(glm::floor(minP * VOXELS_PER_UNIT));
        glm::ivec3 maxVox = glm::ivec3(glm::floor(maxP * VOXELS_PER_UNIT));
        bool collided = false;

        for (int vx = minVox.x; vx <= maxVox.x; vx++) {
            for (int vy = minVox.y; vy <= maxVox.y; vy++) {
                for (int vz = minVox.z; vz <= maxVox.z; vz++) {
                    if (!IsSolidBlock(world.GetVoxel(vx, vy, vz))) continue;

                    float voxMinX = (float)vx / VOXELS_PER_UNIT;
                    float voxMaxX = (float)(vx + 1) / VOXELS_PER_UNIT;
                    float voxMinY = (float)vy / VOXELS_PER_UNIT;
                    float voxMaxY = (float)(vy + 1) / VOXELS_PER_UNIT;
                    float voxMinZ = (float)vz / VOXELS_PER_UNIT;
                    float voxMaxZ = (float)(vz + 1) / VOXELS_PER_UNIT;

                    float overlapX = std::min(maxP.x, voxMaxX) - std::max(minP.x, voxMinX);
                    float overlapY = std::min(maxP.y, voxMaxY) - std::max(minP.y, voxMinY);
                    float overlapZ = std::min(maxP.z, voxMaxZ) - std::max(minP.z, voxMinZ);
                    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) continue;

                    collided = true;
                    if (overlapX < overlapY && overlapX < overlapZ) {
                        float pushSign = (pos.x < (voxMinX + voxMaxX) * 0.5f) ? -1.0f : 1.0f;
                        pos.x += pushSign * overlapX;
                        velocity.x = 0.0f;
                    }
                    else if (overlapY < overlapX && overlapY < overlapZ) {
                        float pushSign = (pos.y < (voxMinY + voxMaxY) * 0.5f) ? -1.0f : 1.0f;
                        pos.y += pushSign * overlapY;
                        velocity.y = 0.0f;
                    }
                    else {
                        float pushSign = (pos.z < (voxMinZ + voxMaxZ) * 0.5f) ? -1.0f : 1.0f;
                        pos.z += pushSign * overlapZ;
                        velocity.z = 0.0f;
                    }
                    // Re-calculate bounds for subsequent checks in this iteration
                    minP = pos - glm::vec3(r);
                    maxP = pos + glm::vec3(r);
                }
            }
        }

        // Keep inside the vertical slab
        float topY = (float)WORLD_HEIGHT / VOXELS_PER_UNIT;
        if (pos.y < r) { pos.y = r; velocity.y = 0.0f; }
        if (pos.y > topY - r) { pos.y = topY - r; velocity.y = 0.0f; }

        if (!collided) break;
    }
}

void FreeFlyCamera::UpdateMovement(GLFWwindow* window, float deltaTime, const VoxelWorld& world) {
    glm::vec3 accelDir = glm::vec3(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) accelDir += m_Front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) accelDir -= m_Front;

    glm::vec3 right = glm::normalize(glm::cross(m_Front, m_Up));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) accelDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) accelDir += right;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) accelDir += m_Up;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) accelDir -= m_Up;

    if (glm::length(accelDir) > 0.0001f) accelDir = glm::normalize(accelDir);

    // Drone flight parameters
    float accelRate = 0.8f;  // World units/s^2
    float dragRate = 3.5f;   // Higher = faster braking
    float maxSpeed = 0.05f;  // World units/s
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { // Boost
        accelRate *= 5.0f;
        maxSpeed *= 4.0f;
    }

    m_Velocity += accelDir * accelRate * deltaTime;
    m_Velocity -= m_Velocity * dragRate * deltaTime;

    float currentSpeed = glm::length(m_Velocity);
    if (currentSpeed > maxSpeed) m_Velocity = glm::normalize(m_Velocity) * maxSpeed;

    // Sliding collision (axis by axis), sub-stepped so no step moves more than half a voxel and
    // the drone cannot tunnel through walls
    if (currentSpeed > 0.0001f) {
        glm::vec3 move = m_Velocity * deltaTime;
        float maxComponent = std::max({ std::abs(move.x), std::abs(move.y), std::abs(move.z) });
        int subSteps = std::max(1, (int)std::ceil(maxComponent * VOXELS_PER_UNIT / 0.5f));

        for (int s = 0; s < subSteps; s++) {
            float stepDt = deltaTime / subSteps;
            for (int axis = 0; axis < 3; axis++) {
                glm::vec3 next = m_Position;
                next[axis] += m_Velocity[axis] * stepDt;
                if (!IsColliding(next, world)) {
                    m_Position[axis] = next[axis];
                } else {
                    m_Velocity[axis] *= -0.2f; // Slight bounce
                }
            }
        }
    }

    // Always resolve, to push the player out of walls it penetrates slightly
    ResolveCollisions(world);
}
