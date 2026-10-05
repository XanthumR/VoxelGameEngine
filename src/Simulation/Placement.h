#pragma once

#include "Simulation/IslandRegistry.h"
#include "World/WorldConstants.h"

#include <glm/glm.hpp>

#include <cstdint>

class OccupancyGrid;
class RoadNetwork;
class VoxelWorld;

// Buildings and roads stand on flat island ground: the first air voxel above the grass
constexpr int BUILD_GROUND_Y = SEA_LEVEL + ISLAND_HEIGHT;
constexpr int ROAD_CLEARANCE = 12; // Voxels of air a road needs above it (people and carts)

enum class PlacementError {
    None,
    NotLoaded,  // Part of the footprint is outside the CPU-loaded world
    Water,      // A column is ocean
    NotFlat,    // A column is not grass at island height (beach, slope)
    Blocked,    // Something solid (a tree, a cliff) stands in the way
    Occupied,   // A building already uses a tile
    Road,       // A road already uses a tile
    TwoIslands, // The footprint spans more than one island
    NeedsCoast, // A producer that must stand by the sea (fishery)
    DockNotOverWater, // The dock part of a coastal building is over land
};

const char* PlacementErrorText(PlacementError error);

struct PlacementCheck {
    PlacementError error = PlacementError::None;
    IslandId island = NO_ISLAND; // The island the whole footprint is on, when valid
};

// What placement looks at
struct PlacementContext {
    const VoxelWorld& world;
    IslandRegistry& islands;
    const OccupancyGrid& occupancy;
    const RoadNetwork& roads;
};

// Can a building of this type and rotation stand with its minimum corner on minTile?
PlacementCheck ValidatePlacement(const PlacementContext& context, uint16_t type, uint8_t rotation, glm::ivec2 minTile);

// Can this tile become road?
PlacementCheck ValidateRoadTile(const PlacementContext& context, glm::ivec2 tile);
