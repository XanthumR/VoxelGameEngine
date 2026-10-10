#pragma once

#include "Economy/PopulationNeeds.h"
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
    Service,   // Public building (marketplace, school, ...): meets a need of the houses in its road reach
    Producer,  // Makes goods (ProductionChains): workforce, inputs, a cart to the warehouse
    Module,    // A farm's pen or field (sheepfold, wheat field): placed near its farm, it makes the farm work (no road needed)
};

// Build menu tab: one per population tier (what that tier unlocks), then Infrastructure
enum class BuildCategory : uint8_t {
    Farmers,
    Workers,
    Artisans,
    Infrastructure,
    Count
};
static_assert((int)BuildCategory::Infrastructure == TIER_COUNT, "a tab per tier");

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
    int8_t moduleOf = -1;  // Modules: the farm type they belong to
    ServiceType service = ServiceType::Count; // Service buildings: the need they meet
};

// Index = BuildingComponent::type
constexpr std::array<BuildingType, 33> BUILDING_TYPES = { {
    { "Warehouse", 4, 4, 18, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Storage, 0, LookStyle::Gable, true, "warehouse", BuildCategory::Infrastructure, -1 },
    { "Farmer House", 3, 3, 14, 30, Block::PLANK, Block::ROOF, BuildingRole::Residence, 0, LookStyle::Gable, true, "farmer_house", BuildCategory::Farmers, -1 },
    { "Marketplace", 4, 3, 10, 24, Block::PLANK, Block::AWNING, BuildingRole::Service, 0, LookStyle::Stall, true, "marketplace", BuildCategory::Farmers, -1, 0, 0, -1, ServiceType::Marketplace },
    { "Worker House", 3, 3, 24, 42, Block::PLANK, Block::ROOF, BuildingRole::Residence, 1, LookStyle::TwoStorey, false, "worker_house", BuildCategory::Workers, -1 },
    { "Fishery", 3, 4, 12, 26, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "fishery", BuildCategory::Farmers, 0, 2, 6 },
    { "Lumberjack", 3, 3, 12, 24, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "lumberjack", BuildCategory::Farmers, 1 },
    { "Sawmill", 3, 3, 12, 26, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Stall, true, "sawmill", BuildCategory::Farmers, 2 },
    { "Sheep Farm", 3, 3, 12, 28, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "sheep_farm", BuildCategory::Farmers, 3 },
    { "Framework Knitter", 3, 3, 20, 34, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::TwoStorey, true, "framework_knitter", BuildCategory::Farmers, 4 },
    { "Pig Farm", 3, 4, 10, 26, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "pig_farm", BuildCategory::Workers, 5 },
    { "Slaughterhouse", 3, 3, 16, 32, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "slaughterhouse", BuildCategory::Workers, 6 },
    // A warehouse on the coast with a pier: ships are built, load and unload here
    { "Harbor", 4, 5, 16, 34, Block::STONE_WALL, Block::ROOF, BuildingRole::Storage, 0, LookStyle::Gable, true, "harbor", BuildCategory::Infrastructure, -1, 2, 6 },
    // Farm modules, as in Anno 1800: fenced pens and fields placed around their farm (from the
    // farm's panel, or right after placing the farm)
    { "Sheepfold", 3, 3, 4, 14, Block::PLANK, Block::ROOF, BuildingRole::Module, 0, LookStyle::Stall, false, "sheepfold", BuildCategory::Farmers, -1, 0, 0, 7 },
    { "Pigsty", 2, 3, 4, 12, Block::PLANK, Block::ROOF, BuildingRole::Module, 0, LookStyle::Stall, false, "pigsty", BuildCategory::Workers, -1, 0, 0, 9 },
    // Milestone 8.1: the rest of the Workers' needs, and the Artisans
    { "School", 3, 3, 16, 38, Block::STONE_WALL, Block::ROOF, BuildingRole::Service, 0, LookStyle::Gable, true, "school", BuildCategory::Workers, -1, 0, 0, -1, ServiceType::School },
    { "Grain Farm", 3, 3, 12, 28, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "grain_farm", BuildCategory::Workers, 7 },
    { "Wheat Field", 3, 3, 4, 10, Block::PLANK, Block::ROOF, BuildingRole::Module, 0, LookStyle::Stall, false, "wheat_field", BuildCategory::Workers, -1, 0, 0, 15 },
    { "Flour Mill", 3, 3, 16, 64, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "flour_mill", BuildCategory::Workers, 8 },
    { "Bakery", 3, 3, 16, 34, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "bakery", BuildCategory::Workers, 9 },
    { "Rendering Works", 3, 3, 14, 34, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "rendering_works", BuildCategory::Workers, 10 },
    { "Soap Factory", 3, 3, 18, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::TwoStorey, true, "soap_factory", BuildCategory::Workers, 11 },
    { "Clay Pit", 3, 3, 8, 24, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Stall, true, "clay_pit", BuildCategory::Workers, 12 },
    { "Brick Factory", 3, 3, 14, 44, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "brick_factory", BuildCategory::Workers, 13 },
    { "Artisan House", 3, 3, 30, 54, Block::STONE_WALL, Block::ROOF, BuildingRole::Residence, 2, LookStyle::TwoStorey, false, "artisan_house", BuildCategory::Artisans, -1 },
    { "Variety Theatre", 4, 4, 22, 50, Block::STONE_WALL, Block::ROOF, BuildingRole::Service, 0, LookStyle::TwoStorey, true, "variety_theatre", BuildCategory::Artisans, -1, 0, 0, -1, ServiceType::Theatre },
    { "Cattle Farm", 3, 3, 12, 28, Block::PLANK, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "cattle_farm", BuildCategory::Artisans, 14 },
    { "Pasture", 3, 3, 4, 12, Block::PLANK, Block::ROOF, BuildingRole::Module, 0, LookStyle::Stall, false, "pasture", BuildCategory::Artisans, -1, 0, 0, 25 },
    { "Iron Mine", 3, 3, 14, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Stall, true, "iron_mine", BuildCategory::Artisans, 15 },
    { "Charcoal Kiln", 3, 3, 10, 28, Block::WOOD, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Stall, true, "charcoal_kiln", BuildCategory::Artisans, 16 },
    { "Furnace", 3, 3, 18, 52, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "furnace", BuildCategory::Artisans, 17 },
    { "Steelworks", 3, 4, 20, 52, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::Gable, true, "steelworks", BuildCategory::Artisans, 18 },
    { "Cannery", 3, 3, 18, 40, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::TwoStorey, true, "cannery", BuildCategory::Artisans, 19 },
    { "Sewing Machine Factory", 3, 3, 22, 46, Block::STONE_WALL, Block::ROOF, BuildingRole::Producer, 0, LookStyle::TwoStorey, true, "sewing_machine_factory", BuildCategory::Artisans, 20 },
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
constexpr uint16_t BUILDING_HARBOR = 11;
constexpr uint16_t BUILDING_SHEEPFOLD = 12;
constexpr uint16_t BUILDING_PIGSTY = 13;
constexpr uint16_t BUILDING_SCHOOL = 14;
constexpr uint16_t BUILDING_GRAIN_FARM = 15;
constexpr uint16_t BUILDING_WHEAT_FIELD = 16;
constexpr uint16_t BUILDING_ARTISAN_HOUSE = 23;
constexpr uint16_t BUILDING_THEATRE = 24;
constexpr uint16_t BUILDING_CATTLE_FARM = 25;
constexpr uint16_t BUILDING_PASTURE = 26;
static_assert(BUILDING_TYPES[BUILDING_SHEEPFOLD].moduleOf == BUILDING_SHEEP_FARM && BUILDING_TYPES[BUILDING_PIGSTY].moduleOf == BUILDING_PIG_FARM &&
              BUILDING_TYPES[BUILDING_WHEAT_FIELD].moduleOf == BUILDING_GRAIN_FARM && BUILDING_TYPES[BUILDING_PASTURE].moduleOf == BUILDING_CATTLE_FARM);
static_assert(BUILDING_TYPES[BUILDING_SCHOOL].service == ServiceType::School && BUILDING_TYPES[BUILDING_THEATRE].service == ServiceType::Theatre);

// The module type of a farm type, or -1
constexpr int ModuleTypeOf(int farmType) {
    for (int type = 0; type < (int)BUILDING_TYPES.size(); type++) {
        if (BUILDING_TYPES[type].moduleOf == farmType) return type;
    }
    return -1;
}

// The residence building of each population tier (upgrades swap between these in place, so they
// must share a footprint)
constexpr std::array<uint16_t, TIER_COUNT> RESIDENCE_FOR_TIER = { BUILDING_FARMER_HOUSE, BUILDING_WORKER_HOUSE, BUILDING_ARTISAN_HOUSE };
constexpr bool ResidencesShareAFootprint() {
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        const BuildingType& house = BUILDING_TYPES[RESIDENCE_FOR_TIER[tier]];
        if (house.footprintWidth != BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintWidth ||
            house.footprintDepth != BUILDING_TYPES[BUILDING_FARMER_HOUSE].footprintDepth || house.role != BuildingRole::Residence || house.tier != tier) {
            return false;
        }
    }
    return true;
}
static_assert(ResidencesShareAFootprint(), "Residence tiers replace each other in place");

// Everything must fit between the ground (SEA_LEVEL + ISLAND_HEIGHT = 46) and the world top (128)
constexpr bool BuildingsFitTheWorld() {
    for (const BuildingType& type : BUILDING_TYPES) {
        if (type.height > 80) return false;
    }
    return true;
}
static_assert(BuildingsFitTheWorld(), "Too tall for the world");
