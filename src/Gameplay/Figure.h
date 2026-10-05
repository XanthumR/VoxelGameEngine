#pragma once

#include <glm/glm.hpp>

// One thing drawn into the voxel world every frame by FigureRenderer (shaders/people/figures.comp):
// a walking person, a smoke puff or a boat. 16 bytes, the std430 layout of the shader's buffer.
struct Figure {
    enum Kind { PERSON = 0, PUFF = 1, BOAT = 2 };

    glm::ivec4 position; // xyz = anchor voxel (a person's feet, a puff's center, a boat's waterline center), w = packed look

    // Person: bits 0-1 tier, 2-3 direction (0 +x, 1 -x, 2 +z, 3 -z), 4-5 walk frame, 6-8 look variant
    static int PackPerson(int tier, int direction, int frame, int variant) {
        return (tier & 3) | ((direction & 3) << 2) | ((frame & 3) << 4) | ((variant & 7) << 6) | (PERSON << 9);
    }
    // Puff: bits 0-1 size - 1 (1-3 voxels), 2-5 density (0-15, how much of the cube is filled), 11-18 seed
    static int PackPuff(int size, int density, int seed) {
        return ((size - 1) & 3) | ((density & 15) << 2) | (PUFF << 9) | ((seed & 255) << 11);
    }
    // Boat: bits 2-3 direction, 4 sail set
    static int PackBoat(int direction, bool sailSet) {
        return ((direction & 3) << 2) | ((sailSet ? 1 : 0) << 4) | (BOAT << 9);
    }
    static int KindOf(int packed) { return (packed >> 9) & 3; }
};
