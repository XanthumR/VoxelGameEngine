#include "Simulation/Placement.h"

#include "Economy/ProductionChains.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/RoadNetwork.h"
#include "World/BlockTypes.h"
#include "World/TerrainGenerator.h"
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
    case PlacementError::DockNotOverWater: return "the dock must reach over the water (R turns it)";
    case PlacementError::NotEnoughCoins: return "not enough coins";
    case PlacementError::NotEnoughPlanks: return "not enough planks on this island";
    }
    return "?";
}

namespace {

// Checks the columns of a rectangle: loaded, grass at island height, `clearance` voxels of
// non-solid space above, and all on one island (also the island already in check, if any)
bool CheckGround(const PlacementContext& context, glm::ivec2 minColumn, glm::ivec2 columns, int clearance, PlacementCheck& check) {
    const VoxelWorld& world = context.world;
    for (int z = minColumn.y; z < minColumn.y + columns.y; z++) {
        for (int x = minColumn.x; x < minColumn.x + columns.x; x++) {
            if (!world.FindChunk(x >> 5, (BUILD_GROUND_Y - 1) >> 5, z >> 5)) {
                check.error = PlacementError::NotLoaded;
                return false;
            }
            if (world.GetVoxel(x, BUILD_GROUND_Y - 1, z) != Block::GRASS) {
                check.error = world.GetVoxel(x, SEA_LEVEL, z) == Block::WATER ? PlacementError::Water : PlacementError::NotFlat;
                return false;
            }
            for (int y = BUILD_GROUND_Y; y < BUILD_GROUND_Y + clearance; y++) {
                if (IsSolidBlock(world.GetVoxel(x, y, z))) {
                    check.error = PlacementError::Blocked;
                    return false;
                }
            }

            IslandId island = context.islands.IslandIdAt(x, z);
            if (island == NO_ISLAND) {
                check.error = PlacementError::NotFlat;
                return false;
            }
            if (check.island != NO_ISLAND && island != check.island) {
                check.error = PlacementError::TwoIslands;
                return false;
            }
            check.island = island;
        }
    }
    return true;
}

// Coastal buildings stand where the coast meanders, so their tiles are counted rather than each
// column checked: land tiles must not be open water (a foundation in the model fills slopes and
// beach under them), dock tiles count their open-water columns. Both need nothing solid in the
// building's space above the ground.
struct ShoreCount {
    int landColumns = 0, islandHeightColumns = 0;
    int dockColumns = 0, waterColumns = 0;
};

bool CheckShoreTile(const PlacementContext& context, glm::ivec2 minColumn, int clearance, bool dock, ShoreCount& count, PlacementCheck& check) {
    const VoxelWorld& world = context.world;
    TerrainGenerator& terrain = context.islands.Terrain();
    for (int z = minColumn.y; z < minColumn.y + TILE_SIZE; z++) {
        for (int x = minColumn.x; x < minColumn.x + TILE_SIZE; x++) {
            if (!world.FindChunk(x >> 5, SEA_LEVEL >> 5, z >> 5)) {
                check.error = PlacementError::NotLoaded;
                return false;
            }
            int height = terrain.TerrainHeightAt(x, z);
            if (dock) {
                count.dockColumns++;
                if (height <= SEA_LEVEL) count.waterColumns++;
            } else {
                if (height <= SEA_LEVEL) {
                    check.error = PlacementError::Water;
                    return false;
                }
                count.landColumns++;
                if (height >= BUILD_GROUND_Y) {
                    count.islandHeightColumns++;
                    IslandId island = context.islands.IslandIdAt(x, z);
                    if (island != NO_ISLAND && check.island != NO_ISLAND && island != check.island) {
                        check.error = PlacementError::TwoIslands;
                        return false;
                    }
                    if (island != NO_ISLAND) check.island = island;
                }
            }
            for (int y = BUILD_GROUND_Y; y < BUILD_GROUND_Y + clearance; y++) {
                if (IsSolidBlock(world.GetVoxel(x, y, z))) {
                    check.error = PlacementError::Blocked;
                    return false;
                }
            }
        }
    }
    return true;
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
    if (building.dockRows == 0) {
        if (!CheckGround(context, minTile * TILE_SIZE, tiles * TILE_SIZE, BuildingHeight(building), check)) return check;
    } else {
        // Coastal: the land rows reach the shore, the dock rows at the back (in the building's own
        // frame) reach over the water
        const int depthTiles = building.footprintDepth;
        ShoreCount count;
        for (int v = 0; v < depthTiles; v++) {
            bool dock = v >= depthTiles - building.dockRows;
            for (int u = 0; u < building.footprintWidth; u++) {
                glm::ivec2 tile = minTile + RotateToFootprint(u, v, rotation, tiles);
                if (!CheckShoreTile(context, tile * TILE_SIZE, BuildingHeight(building), dock, count, check)) return check;
            }
        }
        if (count.islandHeightColumns * 2 < count.landColumns || check.island == NO_ISLAND) {
            check.error = PlacementError::NotFlat; // Not enough of it on the island itself
            return check;
        }
        if (count.waterColumns * 2 < count.dockColumns) {
            check.error = PlacementError::DockNotOverWater;
            return check;
        }
    }

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
    CheckGround(context, tile * TILE_SIZE, glm::ivec2(TILE_SIZE), ROAD_CLEARANCE, check);
    return check;
}
