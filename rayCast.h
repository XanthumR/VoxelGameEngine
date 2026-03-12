#ifndef RAYCAST_H
#define RAYCAST_H


#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

struct RayHit {
    bool hit;
    glm::ivec3 mapPos;
    glm::ivec3 normal; // Useful for placing blocks on faces
};


RayHit raycast(glm::vec3 origin, glm::vec3 dir, float maxDist, int VOXEL_WIDTH, int VOXEL_HEIGHT, int VOXEL_DEPTH,std::vector<GLubyte>& voxelData);


#endif