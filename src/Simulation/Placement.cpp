#include "Simulation/Placement.h"

#include "Simulation/BuildingLook.h"
#include "Simulation/OccupancyGrid.h"
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
    case PlacementError::TwoIslands: return "spans two islands";
    }
    return "?";
}

PlacementCheck ValidatePlacement(uint16_t type, uint8_t rotation, glm::ivec2 minTile, const VoxelWorld& world,
    IslandRegistry& islands, const OccupancyGrid& occupancy) {
    PlacementCheck check;
    if (type >= BUILDING_TYPES.size()) {
        check.error = PlacementError::Blocked;
        return check;
    }
    const BuildingType& building = BUILDING_TYPES[type];
    glm::ivec2 tiles = FootprintTiles(building, rotation);
    if (!occupancy.IsFree(minTile, tiles)) {
        check.error = PlacementError::Occupied;
        return check;
    }

    glm::ivec2 minColumn = minTile * TILE_SIZE;
    glm::ivec2 columns = tiles * TILE_SIZE;
    int height = BuildingHeight(building);
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
            for (int y = BUILD_GROUND_Y; y < BUILD_GROUND_Y + height; y++) {
                if (IsSolidBlock(world.GetVoxel(x, y, z))) {
                    check.error = PlacementError::Blocked;
                    return check;
                }
            }

            IslandId island = islands.IslandIdAt(x, z);
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
