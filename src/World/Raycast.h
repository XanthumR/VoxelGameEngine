#pragma once

#include <glm/glm.hpp>

class VoxelWorld;

struct RayHit {
    bool hit;
    glm::ivec3 mapPos; // Voxel that was hit
    glm::ivec3 normal; // Face that was entered, useful for placing blocks against it
};

// Steps voxel by voxel (DDA) through the CPU world until a solid block. origin and maxDist are
// in world units (1 unit = VOXELS_PER_UNIT voxels). Grass and water are passed through.
RayHit Raycast(const VoxelWorld& world, glm::vec3 origin, glm::vec3 dir, float maxDist);
