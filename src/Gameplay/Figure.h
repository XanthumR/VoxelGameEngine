#pragma once

#include <glm/glm.hpp>

// A smoke puff drawn into the voxel world every frame by FigureRenderer (shaders/people/figures.glsl).
// People, carts and boats are voxel objects (Rendering/VoxelObject.h). 16 bytes, the std430
// layout of the shader's buffer.
struct Figure {
    enum Kind { PUFF = 1 };

    glm::ivec4 position; // xyz = the puff's center, w = packed look

    // Bits 0-1 size - 1 (1-3 voxels), 2-5 density (0-15, how much of the cube is filled), 9 kind, 11-18 seed
    static int PackPuff(int size, int density, int seed) {
        return ((size - 1) & 3) | ((density & 15) << 2) | (PUFF << 9) | ((seed & 255) << 11);
    }
    static int KindOf(int packed) { return (packed >> 9) & 3; }
};
