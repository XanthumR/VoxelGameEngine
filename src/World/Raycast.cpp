#include "World/Raycast.h"

#include "World/BlockTypes.h"
#include "World/VoxelWorld.h"

#include <cmath>

RayHit Raycast(const VoxelWorld& world, glm::vec3 origin, glm::vec3 dir, float maxDist) {
    // Transform origin to voxel space
    glm::vec3 rayPos = origin * VOXELS_PER_UNIT;
    glm::ivec3 mapPos = glm::floor(rayPos);

    glm::vec3 deltaDist = glm::abs(1.0f / dir);
    glm::ivec3 stepDir = glm::sign(dir);
    glm::vec3 sideDist = (glm::sign(dir) * (glm::vec3(mapPos) - rayPos) + (glm::sign(dir) * 0.5f) + 0.5f) * deltaDist;

    glm::ivec3 lastNormal(0);

    // A DDA visits at most ~3 cells per voxel of travel
    float maxVoxDist = maxDist * VOXELS_PER_UNIT;
    int maxSteps = (int)std::ceil(maxVoxDist * 3.0f) + 1;

    for (int i = 0; i < maxSteps; i++) {
        if (mapPos.y < 0 || mapPos.y >= WORLD_HEIGHT) break;
        if (glm::length(glm::vec3(mapPos) - rayPos) > maxVoxDist + 1.0f) break;

        if (IsSolidBlock(world.GetVoxel(mapPos.x, mapPos.y, mapPos.z))) {
            return { true, mapPos, lastNormal };
        }

        // Step into the neighbouring voxel whose wall is closest along the ray
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                sideDist.x += deltaDist.x; mapPos.x += stepDir.x;
                lastNormal = glm::ivec3(-stepDir.x, 0, 0);
            }
            else {
                sideDist.z += deltaDist.z; mapPos.z += stepDir.z;
                lastNormal = glm::ivec3(0, 0, -stepDir.z);
            }
        }
        else {
            if (sideDist.y < sideDist.z) {
                sideDist.y += deltaDist.y; mapPos.y += stepDir.y;
                lastNormal = glm::ivec3(0, -stepDir.y, 0);
            }
            else {
                sideDist.z += deltaDist.z; mapPos.z += stepDir.z;
                lastNormal = glm::ivec3(0, 0, -stepDir.z);
            }
        }
    }
    return { false, glm::ivec3(0), glm::ivec3(0) };
}
