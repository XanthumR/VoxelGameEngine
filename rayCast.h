#ifndef RAYCAST_H
#define RAYCAST_H

#include <glm/glm.hpp>

struct RayHit {
    bool hit;
    glm::ivec3 mapPos;
    glm::ivec3 normal; // Useful for placing blocks on faces
};

// origin and maxDist are in world units (1 unit = VOXELS_PER_UNIT voxels)
RayHit raycast(glm::vec3 origin, glm::vec3 dir, float maxDist);

#endif
