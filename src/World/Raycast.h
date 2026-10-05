#pragma once

#include <glm/glm.hpp>

class VoxelWorld;

struct RayHit {
    bool hit;
    glm::ivec3 mapPos; // Voxel that was hit
    glm::ivec3 normal; // Face that was entered, useful for placing blocks against it
};

// Steps voxel by voxel (DDA) through the CPU world until a solid block, or also water when
// stopAtWater is set. Grass tufts are always passed through. origin and maxDist are in world
// units (1 unit = VOXELS_PER_UNIT voxels); a ray starting above or below the world enters it
// first. Only chunks with CPU data (near the camera) can be hit.
RayHit Raycast(const VoxelWorld& world, glm::vec3 origin, glm::vec3 dir, float maxDist, bool stopAtWater = false);
