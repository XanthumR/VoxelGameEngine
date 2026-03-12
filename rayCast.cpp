#include "rayCast.h"


RayHit raycast(glm::vec3 origin, glm::vec3 dir, float maxDist, int VOXEL_WIDTH, int VOXEL_HEIGHT, int VOXEL_DEPTH,std::vector<GLubyte>& voxelData) {
    // Transform origin to grid space (0.0 to dimX)
    glm::vec3 rayPos = origin * (float)VOXEL_WIDTH;
    glm::ivec3 mapPos = glm::floor(rayPos);

    glm::vec3 deltaDist = glm::abs(1.0f / dir);
    glm::ivec3 stepDir = glm::sign(dir);
    glm::vec3 sideDist = (glm::sign(dir) * (glm::vec3(mapPos) - rayPos) + (glm::sign(dir) * 0.5f) + 0.5f) * deltaDist;

    glm::ivec3 lastNormal(0);

    for (int i = 0; i < 200; i++) { // Max reach distance
        // Check bounds
        if (mapPos.x < 0 || mapPos.x >= VOXEL_WIDTH ||
            mapPos.y < 0 || mapPos.y >= VOXEL_HEIGHT ||
            mapPos.z < 0 || mapPos.z >= VOXEL_DEPTH) break;

        // Check distance
        if (glm::length(glm::vec3(mapPos) - rayPos) > maxDist * VOXEL_WIDTH) break;

        // Sample our CPU data
        size_t idx = ((size_t)mapPos.z * VOXEL_WIDTH * VOXEL_HEIGHT) + ((size_t)mapPos.y * VOXEL_WIDTH) + mapPos.x;
        if (voxelData[idx] > 0) {
            return { true, mapPos, lastNormal };
        }

        // Step
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