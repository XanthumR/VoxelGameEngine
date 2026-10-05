#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

// A building's voxels from a MagicaVoxel (.vox) file, as a dense grid in the building's own
// frame: u across the front, v from the front (0) to the back, y up from the ground. Palette
// indices are block IDs as they are (see assets/buildings/README.md).
//
// MagicaVoxel is z-up; the door side is its y = 0 side. Mapping its x to width - 1 - u keeps the
// model's handedness, so it looks the same from the front in both programs.
struct BuildingModel {
    int width = 0, depth = 0, height = 0; // u, v, y
    std::vector<uint8_t> ids;             // u fastest, then v, then y

    // Returns false (and leaves the model empty) if the file is missing or invalid
    bool Load(const std::string& path);

    uint8_t At(int u, int v, int y) const { return ids[(size_t)u + (size_t)width * ((size_t)v + (size_t)depth * (size_t)y)]; }
    bool IsEmpty() const { return ids.empty(); }
};
