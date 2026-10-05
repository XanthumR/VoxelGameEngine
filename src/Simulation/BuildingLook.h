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

// Voxels from the ground to the roof ridge
int BuildingHeight(const BuildingType& type);

// Fills ids with the building's volume (FootprintColumns x BuildingHeight), AIR where empty.
// Order: x fastest, then z, then y (from the ground up).
void BuildLook(const BuildingType& type, uint8_t rotation, std::vector<uint8_t>& ids);
