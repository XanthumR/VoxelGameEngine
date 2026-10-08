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

// A building going up, progress 0 (nothing) to 1 (finished): the lower layers are finished, with a
// ragged top edge, and above them a timber frame stands where the next layers go; the rest is air.
// look is the finished volume of the given size (x, height, z; ordered like BuildLook); out gets
// the same size.
void ConstructionLook(const std::vector<uint8_t>& look, glm::ivec3 size, float progress, std::vector<uint8_t>& out);

// A demolished building coming down, progress 0 (standing) to 1 (gone): the top gives way first and
// every voxel falls, faster and faster, into a mound of rubble that grows on the footprint and then
// sinks away. Until progress 1 every column keeps at least one rubble voxel on the ground, so
// nothing can be built there before the end. The lowest groundLayer layers (pilings under the
// ground or sea) stay as they are.
void DemolitionLook(const std::vector<uint8_t>& look, glm::ivec3 size, int groundLayer, float progress, std::vector<uint8_t>& out);
