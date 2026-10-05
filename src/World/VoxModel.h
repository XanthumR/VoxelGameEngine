#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct VoxelOffset {
    int x, y, z;
    int blockType;
};

// A MagicaVoxel (.vox) model as voxel offsets from its root: centered on X/Z, base at y = 0.
// Used for the trees stamped into the terrain.
struct VoxModel {
    std::vector<VoxelOffset> voxels;
    glm::ivec3 min = glm::ivec3(0); // Bounds of the offsets
    glm::ivec3 max = glm::ivec3(0);

    // Returns false (and leaves the model empty) if the file is missing or invalid
    bool Load(const std::string& path);
    bool IsEmpty() const { return voxels.empty(); }
};
