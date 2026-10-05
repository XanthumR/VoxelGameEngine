#pragma once

#include "World/BlockTypes.h"

#include <array>
#include <cstdint>

// Buildings sit on a grid of build tiles, TILE_SIZE x TILE_SIZE columns each. Footprints, the
// occupancy grid and placement snapping all work in tiles.
constexpr int TILE_SIZE = 4;

// Floor division of a world column to its tile (also for negative columns)
constexpr int ColumnToTile(int column) { return column >= 0 ? column / TILE_SIZE : -((-column + TILE_SIZE - 1) / TILE_SIZE); }

struct BuildingType {
    const char* name;
    int footprintWidth; // Tiles along the front (the side with the door)
    int footprintDepth; // Tiles from front to back
    int wallHeight;     // Voxels; a gable roof sits on top (see BuildingLook)
    uint8_t wallBlock;
    uint8_t roofBlock;
};

// Index = BuildingComponent::type
constexpr std::array<BuildingType, 2> BUILDING_TYPES = { {
    { "Warehouse", 4, 4, 6, Block::STONE_WALL, Block::ROOF },
    { "Farmer House", 3, 3, 5, Block::PLANK, Block::ROOF },
} };
constexpr uint16_t BUILDING_WAREHOUSE = 0;
constexpr uint16_t BUILDING_FARMER_HOUSE = 1;
