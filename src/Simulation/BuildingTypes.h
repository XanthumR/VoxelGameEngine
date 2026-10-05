#pragma once

#include "World/BlockTypes.h"

#include <array>
#include <cstdint>

// Buildings sit on a grid of build tiles, TILE_SIZE x TILE_SIZE columns each. Footprints, the
// occupancy grid and placement snapping all work in tiles.
constexpr int TILE_SIZE = 12;

// Floor division of a world column to its tile (also for negative columns)
constexpr int ColumnToTile(int column) { return column >= 0 ? column / TILE_SIZE : -((-column + TILE_SIZE - 1) / TILE_SIZE); }

// What a building does
enum class BuildingRole : uint8_t {
    Storage,   // Warehouse: island storage capacity, start of road reach
    Residence, // House: residents of one population tier
    Market,    // Marketplace: houses in its road reach get goods
};

// How BuildLook draws it
enum class LookStyle : uint8_t {
    Gable,     // Walls, door, windows, gable roof
    TwoStorey, // Masonry ground floor, walls above, gable roof
    Stall,     // Corner posts, a low counter, awning roof
};

struct BuildingType {
    const char* name;
    int footprintWidth; // Tiles along the front (the side with the door)
    int footprintDepth; // Tiles from front to back
    int wallHeight;     // Voxels (procedural look); a gable roof sits on top (see BuildingLook)
    int height;         // Voxels from the ground to the top: placement clearance, preview, models
    uint8_t wallBlock;
    uint8_t roofBlock;
    BuildingRole role;
    uint8_t tier;       // Residences: index into POPULATION_TIERS (src/Economy/PopulationNeeds.h)
    LookStyle style;
    bool buildable;     // In the build menu (upgrades only otherwise)
};

// Index = BuildingComponent::type
constexpr std::array<BuildingType, 4> BUILDING_TYPES = { {
    { "Warehouse", 4, 4, 18, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Storage, 0, LookStyle::Gable, true },
    { "Farmer House", 3, 3, 14, 30, Block::PLANK, Block::ROOF, BuildingRole::Residence, 0, LookStyle::Gable, true },
    { "Marketplace", 4, 3, 10, 24, Block::PLANK, Block::AWNING, BuildingRole::Market, 0, LookStyle::Stall, true },
    { "Worker House", 3, 3, 24, 42, Block::PLANK, Block::ROOF, BuildingRole::Residence, 1, LookStyle::TwoStorey, false },
} };
constexpr uint16_t BUILDING_WAREHOUSE = 0;
constexpr uint16_t BUILDING_FARMER_HOUSE = 1;
constexpr uint16_t BUILDING_MARKETPLACE = 2;
constexpr uint16_t BUILDING_WORKER_HOUSE = 3;

// The residence building of each population tier (upgrades swap between these in place, so they
// must share a footprint)
constexpr std::array<uint16_t, 2> RESIDENCE_FOR_TIER = { BUILDING_FARMER_HOUSE, BUILDING_WORKER_HOUSE };
static_assert(BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintWidth == BUILDING_TYPES[BUILDING_WORKER_HOUSE].footprintWidth &&
              BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintDepth == BUILDING_TYPES[BUILDING_WORKER_HOUSE].footprintDepth,
    "Residence tiers replace each other in place");

// Everything must fit between the ground (SEA_LEVEL + ISLAND_HEIGHT = 46) and the world top (128)
static_assert(BUILDING_TYPES[BUILDING_WAREHOUSE].height <= 80 && BUILDING_TYPES[BUILDING_WORKER_HOUSE].height <= 80, "Too tall for the world");
