#pragma once

#include "Simulation/BuildingTypes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// The voxels a building is drawn with. A building is a GameObject; these voxels are only its look,
// stamped into the world when it is placed and cleared when it is demolished.
//
// Rotation is in quarter turns and decides which way the front (door) faces:
// 0 = -z, 1 = +x, 2 = +z, 3 = -x.

// Footprint in tiles / columns along world x and z, after rotation
glm::ivec2 FootprintTiles(const BuildingType& type, uint8_t rotation);
glm::ivec2 FootprintColumns(const BuildingType& type, uint8_t rotation);

// Voxels from the ground to the top
int BuildingHeight(const BuildingType& type);

// The whole stamped volume, from belowGround voxels under the ground to the top
inline int BuildingVolumeHeight(const BuildingType& type) { return type.belowGround + type.height; }

// A column of the building's own frame (u across the front, v from the front) in the rotated
// footprint of the given size (FootprintColumns): the front ends up facing the rotation's direction
inline glm::ivec2 RotateToFootprint(int u, int v, uint8_t rotation, glm::ivec2 size) {
    switch (rotation & 3) {
    case 0: return glm::ivec2(u, v);
    case 1: return glm::ivec2(size.x - 1 - v, u);
    case 2: return glm::ivec2(size.x - 1 - u, size.y - 1 - v);
    default: return glm::ivec2(v, size.y - 1 - u);
    }
}

// Fills ids with the building's volume (FootprintColumns x BuildingHeight), AIR where empty.
// Order: x fastest, then z, then y (from the ground up).
void BuildLook(const BuildingType& type, uint8_t rotation, std::vector<uint8_t>& ids);
