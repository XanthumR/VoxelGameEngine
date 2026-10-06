#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// A moving thing drawn as its own voxel model with a free position and rotation (like Teardown):
// the shade pass ray-marches the model inside its oriented bounding box, so it can face any way and
// tilt, off the world's grid. Models are registered once (VoxelRenderer::AddObjectModel).
// Local frame: x right, y up, z forward.
struct VoxelObject {
    int model = 0;
    glm::vec3 position{ 0.0f }; // World voxels: the middle of the model's bottom face
    float yaw = 0.0f;           // Radians; 0 faces +z, a quarter turn faces +x
    float pitch = 0.0f;         // Nose up
    float roll = 0.0f;          // Right side down
};

// Block IDs filling size, x fastest, then z, then y (the order building models use); 0 = empty
struct VoxelObjectModel {
    glm::ivec3 size{ 0 };
    std::vector<uint8_t> ids;
};
