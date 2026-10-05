#pragma once

#include <glm/glm.hpp>

#include <cstdint>

class ICamera;
class VoxelWorld;

// What the mouse cursor points at in the world
struct PickResult {
    bool hit = false;
    glm::ivec3 voxel = glm::ivec3(0);  // The surface voxel under the cursor (water counts)
    glm::ivec3 normal = glm::ivec3(0); // Face of that voxel facing the camera
    uint8_t block = 0;
};

// Direction (world space, normalized) of the camera ray through a window pixel. Uses the same
// projection as the renderer (60 degree vertical field of view) and its pixel-to-ray math.
glm::vec3 ScreenToWorldDirection(const ICamera& camera, glm::vec2 cursor, glm::ivec2 windowSize);

// The window pixel a world point (world units) projects to; false when it is behind the camera.
// The inverse of ScreenToWorldDirection.
bool WorldToScreen(const ICamera& camera, glm::vec3 worldPosition, glm::ivec2 windowSize, glm::vec2& screen);

// Casts that ray into the CPU world (chunks near the camera only)
PickResult PickUnderCursor(const ICamera& camera, glm::vec2 cursor, glm::ivec2 windowSize, const VoxelWorld& world);
