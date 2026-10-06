#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// What the renderer highlights (shade.comp): the building about to be placed, drawn as a
// see-through ghost of its model tinted green or red, or the building under the cursor (a box)
struct BuildPreview {
    enum State { NONE = 0, VALID = 1, INVALID = 2, SELECTED = 3 }; // Off, green, red, yellow
    int state = NONE;
    glm::ivec3 min = glm::ivec3(0); // Voxels, inclusive
    glm::ivec3 max = glm::ivec3(0); // Voxels, exclusive
    // VALID/INVALID: the model filling min..max (x fastest, then z, then y, as
    // BuildingModelLibrary::BuildVoxels writes it); ghostRevision changes whenever it does
    const std::vector<uint8_t>* ghost = nullptr;
    uint32_t ghostRevision = 0;
};
