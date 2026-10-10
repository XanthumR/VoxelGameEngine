#pragma once

#include <glm/glm.hpp>

#include <algorithm>
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
    // > 0: it floats on the ocean's waves, its bottom this many voxels below the waterline. The
    // GPU then sets its height, pitch and roll from the waves under it (shaders/render/float.comp);
    // position.y, pitch and roll here are ignored.
    float waterline = 0.0f;
    uint8_t cargo = 0; // The block its model's Block::CARGO voxels are drawn as (carts)
};

// Where an object's shadow can fall: the xz rectangle (low, high) of its box and of the box pushed
// along -lightDir down to receiverY (the lowest surface that can be shadowed), at most maxPush
// voxels. axisX/Y/Z are the object's local axes in the world, position the middle of its bottom.
inline void ObjectShadowFootprint(glm::vec3 position, glm::vec3 axisX, glm::vec3 axisY, glm::vec3 axisZ, glm::ivec3 size,
    glm::vec3 lightDir, float receiverY, float maxPush, glm::vec2& low, glm::vec2& high) {
    low = glm::vec2(1e9f);
    high = glm::vec2(-1e9f);
    for (int corner = 0; corner < 8; corner++) {
        glm::vec3 local((corner & 1) ? 0.5f : -0.5f, (corner & 2) ? 1.0f : 0.0f, (corner & 4) ? 0.5f : -0.5f);
        local *= glm::vec3(size);
        glm::vec3 world = position + axisX * local.x + axisY * local.y + axisZ * local.z;
        float push = lightDir.y > 0.01f ? std::min((world.y - receiverY) / lightDir.y, maxPush) : maxPush;
        glm::vec2 shadow = glm::vec2(world.x, world.z) - glm::vec2(lightDir.x, lightDir.z) * std::max(push, 0.0f);
        low = glm::min(low, glm::min(glm::vec2(world.x, world.z), shadow));
        high = glm::max(high, glm::max(glm::vec2(world.x, world.z), shadow));
    }
}

// Block IDs filling size, x fastest, then z, then y (the order building models use); 0 = empty
struct VoxelObjectModel {
    glm::ivec3 size{ 0 };
    std::vector<uint8_t> ids;
};
