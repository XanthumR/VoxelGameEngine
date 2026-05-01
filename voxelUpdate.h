#ifndef VOXELUPDATE_H
#define VOXELUPDATE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

struct VoxelUpdate {
    int x, y, z;
    GLubyte id;
};



void queueVoxelUpdate(glm::ivec3 pos, GLubyte id, int VOXEL_WIDTH, int VOXEL_HEIGHT, int VOXEL_DEPTH, std::vector<GLubyte>& voxelData);

void flushVoxelUpdates(GLuint voxelTex);








#endif