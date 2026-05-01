#include "voxelUpdate.h"

std::vector<VoxelUpdate> pendingUpdates;

void queueVoxelUpdate(glm::ivec3 pos, GLubyte id, int VOXEL_WIDTH, int VOXEL_HEIGHT, int VOXEL_DEPTH, std::vector<GLubyte>& voxelData) {
    // 1. Update CPU Array

    size_t Idx = (pos.z * VOXEL_WIDTH * VOXEL_HEIGHT) + (pos.y * VOXEL_WIDTH) + pos.x;

    voxelData[Idx] = id;

    // 2. Queue for GPU upload

    pendingUpdates.push_back({ pos.x, pos.y, pos.z, id });
}

void flushVoxelUpdates(GLuint voxelTex) {
    if (pendingUpdates.empty()) return;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, voxelTex);

    for (const auto& update : pendingUpdates) {
        glTexSubImage3D(GL_TEXTURE_3D, 0,
            update.x, update.y, update.z,
            1, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &update.id);
    }

    pendingUpdates.clear();
}