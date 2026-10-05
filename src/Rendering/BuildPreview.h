#pragma once

#include <glm/glm.hpp>

// A box the renderer highlights (shade.comp): the footprint of the building about to be placed,
// or the building under the cursor
struct BuildPreview {
    enum State { NONE = 0, VALID = 1, INVALID = 2, SELECTED = 3 }; // Off, green, red, yellow
    int state = NONE;
    glm::ivec3 min = glm::ivec3(0); // Voxels, inclusive
    glm::ivec3 max = glm::ivec3(0); // Voxels, exclusive
};
