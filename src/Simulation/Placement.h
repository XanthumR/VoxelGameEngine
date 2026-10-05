#pragma once

#include "Simulation/IslandRegistry.h"
#include "World/WorldConstants.h"

#include <glm/glm.hpp>

#include <cstdint>

class OccupancyGrid;
class VoxelWorld;

// Buildings stand on flat island ground: the first air voxel above the grass
constexpr int BUILD_GROUND_Y = SEA_LEVEL + ISLAND_HEIGHT;

enum class PlacementError {
    None,
    NotLoaded,  // Part of the footprint is outside the CPU-loaded world
    Water,      // A column is ocean
    NotFlat,    // A column is not grass at island height (beach, slope)
    Blocked,    // Something solid (a tree, a cliff) stands in the building's volume
    Occupied,   // Another building already uses a tile
    TwoIslands, // The footprint spans more than one island
};

const char* PlacementErrorText(PlacementError error);

struct PlacementCheck {
    PlacementError error = PlacementError::None;
    IslandId island = NO_ISLAND; // The island the whole footprint is on, when valid
};

// Can a building of this type and rotation stand with its minimum corner on minTile?
PlacementCheck ValidatePlacement(uint16_t type, uint8_t rotation, glm::ivec2 minTile, const VoxelWorld& world,
    IslandRegistry& islands, const OccupancyGrid& occupancy);
