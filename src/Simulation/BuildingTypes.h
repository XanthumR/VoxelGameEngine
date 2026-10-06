#pragma once

#include "World/BlockTypes.h"
#include "World/WorldConstants.h"

#include <array>
#include <cstdint>

// Buildings sit on the grid of build tiles (TILE_SIZE in World/WorldConstants.h). Footprints, the
// occupancy grid and placement snapping all work in tiles.

// What a building does
enum class BuildingRole : uint8_t {
    Storage,   // Warehouse: island storage capacity, start of road reach
    Residence, // House: residents of one population tier
    Market,    // Marketplace: houses in its road reach get goods
    Producer,  // Makes goods (ProductionChains): workforce, inputs, a cart to the warehouse
};

// Build menu tab
enum class BuildCategory : uint8_t {
    Housing,
    Production,
    Infrastructure,
    Count
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
    const char* modelName; // assets/buildings/<modelName>_<n>.vox (BuildingModelLibrary); nullptr: procedural only
    BuildCategory category;
    int8_t chain;          // Producers: index into PRODUCTION_CHAINS (src/Economy/ProductionChains.h), else -1
    int dockRows = 0;      // Tiles at the back of the footprint that stand over the water (a dock)
    int belowGround = 0;   // Voxels the look reaches below the ground (pilings, a moored hull)
};

// Index = BuildingComponent::type
constexpr std::array<BuildingType, 11> BUILDING_TYPES = { {
    { "Warehouse", 4, 4, 18, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Storage, 0, LookStyle::Gable, true, "warehouse", BuildCategory::Infrastructure, -1 },
    { "Farmer House", 3, 3, 14, 30, Block::PLANK, Block::ROOF, BuildingRole::Residence, 0, LookStyle::Gable, true, "farmer_house", BuildCategory::Housing, -1 },
    { "Marketplace", 4, 3, 10, 24, Block::PLANK, Block::AWNING, BuildingRole::Market, 0, LookStyle::Stall, true, "marketplace", BuildCategory::Housing, -1 },
    { "Worker House", 3, 3, 24, 42, Block::PLANK, Block::ROOF, BuildingRole::Residence, 1, LookStyle::TwoStorey, false, "worker_house", BuildCategory::Housing, -1 },
    { "Fishery", 3, 4, 12, 26, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "fishery", BuildCategory::Production, 0, 2, 6 },
    { "Lumberjack", 3, 3, 12, 24, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "lumberjack", BuildCategory::Production, 1 },
    { "Sawmill", 3, 3, 12, 26, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Stall, true, "sawmill", BuildCategory::Production, 2 },
    { "Sheep Farm", 3, 3, 12, 28, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "sheep_farm", BuildCategory::Production, 3 },
    { "Framework Knitter", 3, 3, 20, 34, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::TwoStorey, true, "framework_knitter", BuildCategory::Production, 4 },
    { "Pig Farm", 3, 3, 10, 22, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "pig_farm", BuildCategory::Production, 5 },
    { "Slaughterhouse", 3, 3, 16, 32, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "slaughterhouse", BuildCategory::Production, 6 },
} };
constexpr uint16_t BUILDING_WAREHOUSE = 0;
constexpr uint16_t BUILDING_FARMER_HOUSE = 1;
constexpr uint16_t BUILDING_MARKETPLACE = 2;
constexpr uint16_t BUILDING_WORKER_HOUSE = 3;
constexpr uint16_t BUILDING_FISHERY = 4;
constexpr uint16_t BUILDING_LUMBERJACK = 5;
constexpr uint16_t BUILDING_SAWMILL = 6;
constexpr uint16_t BUILDING_SHEEP_FARM = 7;
constexpr uint16_t BUILDING_FRAMEWORK_KNITTER = 8;
constexpr uint16_t BUILDING_PIG_FARM = 9;
constexpr uint16_t BUILDING_SLAUGHTERHOUSE = 10;

// The residence building of each population tier (upgrades swap between these in place, so they
// must share a footprint)
constexpr std::array<uint16_t, 2> RESIDENCE_FOR_TIER = { BUILDING_FARMER_HOUSE, BUILDING_WORKER_HOUSE };
static_assert(BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintWidth == BUILDING_TYPES[BUILDING_WORKER_HOUSE].footprintWidth &&
              BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintDepth == BUILDING_TYPES[BUILDING_WORKER_HOUSE].footprintDepth,
    "Residence tiers replace each other in place");

// Everything must fit between the ground (SEA_LEVEL + ISLAND_HEIGHT = 46) and the world top (128)
static_assert(BUILDING_TYPES[BUILDING_WAREHOUSE].height <= 80 && BUILDING_TYPES[BUILDING_WORKER_HOUSE].height <= 80, "Too tall for the world");
