#include "Simulation/Placement.h"

#include "Economy/ProductionChains.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/RoadNetwork.h"
#include "World/BlockTypes.h"
#include "World/VoxelWorld.h"

const char* PlacementErrorText(PlacementError error) {
    switch (error) {
    case PlacementError::None: return "OK";
    case PlacementError::NotLoaded: return "not loaded";
    case PlacementError::Water: return "water";
    case PlacementError::NotFlat: return "ground not flat";
    case PlacementError::Blocked: return "blocked";
    case PlacementError::Occupied: return "occupied";
    case PlacementError::Road: return "road in the way";
    case PlacementError::TwoIslands: return "spans two islands";
    case PlacementError::NeedsCoast: return "must be at the coast";
    }
    return "?";
}

namespace {

// Checks the columns of a rectangle: loaded, grass at island height, `clearance` voxels of
// non-solid space above, and all on one island
PlacementCheck CheckGround(const PlacementContext& context, glm::ivec2 minColumn, glm::ivec2 columns, int clearance) {
    PlacementCheck check;
    const VoxelWorld& world = context.world;
    for (int z = minColumn.y; z < minColumn.y + columns.y; z++) {
        for (int x = minColumn.x; x < minColumn.x + columns.x; x++) {
            if (!world.FindChunk(x >> 5, (BUILD_GROUND_Y - 1) >> 5, z >> 5)) {
                check.error = PlacementError::NotLoaded;
                return check;
            }
            if (world.GetVoxel(x, BUILD_GROUND_Y - 1, z) != Block::GRASS) {
                check.error = world.GetVoxel(x, SEA_LEVEL, z) == Block::WATER ? PlacementError::Water : PlacementError::NotFlat;
                return check;
            }
            for (int y = BUILD_GROUND_Y; y < BUILD_GROUND_Y + clearance; y++) {
                if (IsSolidBlock(world.GetVoxel(x, y, z))) {
                    check.error = PlacementError::Blocked;
                    return check;
                }
            }

            IslandId island = context.islands.IslandIdAt(x, z);
            if (island == NO_ISLAND) {
                check.error = PlacementError::NotFlat;
                return check;
            }
            if (check.island != NO_ISLAND && island != check.island) {
                check.error = PlacementError::TwoIslands;
                return check;
            }
            check.island = island;
        }
    }
    return check;
}

} // namespace

PlacementCheck ValidatePlacement(const PlacementContext& context, uint16_t type, uint8_t rotation, glm::ivec2 minTile) {
    PlacementCheck check;
    if (type >= BUILDING_TYPES.size()) {
        check.error = PlacementError::Blocked;
        return check;
    }
    const BuildingType& building = BUILDING_TYPES[type];
    glm::ivec2 tiles = FootprintTiles(building, rotation);
    if (!context.occupancy.IsFree(minTile, tiles)) {
        check.error = PlacementError::Occupied;
        return check;
    }
    for (int z = 0; z < tiles.y; z++) {
        for (int x = 0; x < tiles.x; x++) {
            if (context.roads.IsRoad(minTile + glm::ivec2(x, z))) {
                check.error = PlacementError::Road;
                return check;
            }
        }
    }
    check = CheckGround(context, minTile * TILE_SIZE, tiles * TILE_SIZE, BuildingHeight(building));
    if (check.error != PlacementError::None) return check;

    // Location rules that forbid a spot (the others only change productivity)
    if (building.role == BuildingRole::Producer) {
        const ProductionChain& chain = PRODUCTION_CHAINS[building.chain];
        if (chain.rule == LocationRule::Coast && !HasCoast(context.islands.Terrain(), minTile, tiles, chain.radius)) {
            check.error = PlacementError::NeedsCoast;
        }
    }
    return check;
}

PlacementCheck ValidateRoadTile(const PlacementContext& context, glm::ivec2 tile) {
    PlacementCheck check;
    if (context.roads.IsRoad(tile)) {
        check.error = PlacementError::Road;
        return check;
    }
    if (context.occupancy.At(tile) != INVALID_GAME_OBJECT) {
        check.error = PlacementError::Occupied;
        return check;
    }
    return CheckGround(context, tile * TILE_SIZE, glm::ivec2(TILE_SIZE), ROAD_CLEARANCE);
}
