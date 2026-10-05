#include "Simulation/BuildingLook.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "TestWorld.h"
#include "World/BlockTypes.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

namespace {

glm::ivec2 SpawnTile() {
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    return glm::ivec2(ColumnToTile(spawn.x), ColumnToTile(spawn.y));
}

// Nearest tile (searching outwards from spawn) where the building fits on an empty grid
bool FindValidTile(uint16_t type, IslandRegistry& islands, const OccupancyGrid& occupancy, glm::ivec2& tile) {
    TestWorld& test = TestWorld::Get();
    for (int ring = 0; ring < 20; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                glm::ivec2 candidate = SpawnTile() + glm::ivec2(dx, dz);
                if (ValidatePlacement(type, 0, candidate, test.world, islands, occupancy).error == PlacementError::None) {
                    tile = candidate;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST(PlacementTest, FlatIslandGroundIsValid) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(BUILDING_WAREHOUSE, islands, occupancy, tile));

    PlacementCheck check = ValidatePlacement(BUILDING_WAREHOUSE, 0, tile, test.world, islands, occupancy);
    EXPECT_EQ(check.error, PlacementError::None);
    EXPECT_EQ(check.island, islands.IslandIdAt(test.spawnColumn.x, test.spawnColumn.y));
}

TEST(PlacementTest, OccupiedTilesAreRejected) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(BUILDING_WAREHOUSE, islands, occupancy, tile));

    occupancy.Occupy(tile, FootprintTiles(BUILDING_TYPES[BUILDING_WAREHOUSE], 0), 1);
    EXPECT_EQ(ValidatePlacement(BUILDING_WAREHOUSE, 0, tile, test.world, islands, occupancy).error, PlacementError::Occupied);
    // A house overlapping one corner tile
    EXPECT_EQ(ValidatePlacement(BUILDING_FARMER_HOUSE, 0, tile + glm::ivec2(3, 3), test.world, islands, occupancy).error,
        PlacementError::Occupied);
}

TEST(PlacementTest, WaterIsRejected) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    // A loaded tile whose first column is open water
    int reach = TestWorld::RADIUS_CHUNKS * 32 - 32;
    bool found = false;
    for (int dz = -reach; dz < reach && !found; dz += TILE_SIZE) {
        for (int dx = -reach; dx < reach && !found; dx += TILE_SIZE) {
            glm::ivec2 tile = SpawnTile() + glm::ivec2(dx, dz) / TILE_SIZE;
            glm::ivec2 column = tile * TILE_SIZE;
            if (test.world.GetVoxel(column.x, SEA_LEVEL, column.y) != Block::WATER) continue;
            if (test.world.GetVoxel(column.x, SEA_LEVEL + 1, column.y) != Block::AIR) continue;
            EXPECT_EQ(ValidatePlacement(BUILDING_FARMER_HOUSE, 0, tile, test.world, islands, occupancy).error, PlacementError::Water);
            found = true;
        }
    }
    EXPECT_TRUE(found) << "No water near the spawn island";
}

TEST(PlacementTest, SomethingSolidInTheVolumeBlocksIt) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(BUILDING_FARMER_HOUSE, islands, occupancy, tile));

    glm::ivec3 rock(tile.x * TILE_SIZE + 5, BUILD_GROUND_Y + 2, tile.y * TILE_SIZE + 5);
    ASSERT_TRUE(test.world.SetVoxel(rock.x, rock.y, rock.z, Block::STONE));
    EXPECT_EQ(ValidatePlacement(BUILDING_FARMER_HOUSE, 0, tile, test.world, islands, occupancy).error, PlacementError::Blocked);
    test.world.SetVoxel(rock.x, rock.y, rock.z, Block::AIR); // The world is shared by other tests
    EXPECT_EQ(ValidatePlacement(BUILDING_FARMER_HOUSE, 0, tile, test.world, islands, occupancy).error, PlacementError::None);
}

TEST(PlacementTest, UnloadedWorldIsRejected) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    glm::ivec2 farAway = SpawnTile() + glm::ivec2(1000, 0);
    EXPECT_EQ(ValidatePlacement(BUILDING_FARMER_HOUSE, 0, farAway, test.world, islands, occupancy).error, PlacementError::NotLoaded);
}

TEST(PlacementTest, UnknownTypeIsRejected) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    EXPECT_NE(ValidatePlacement((uint16_t)BUILDING_TYPES.size(), 0, SpawnTile(), test.world, islands, occupancy).error,
        PlacementError::None);
}
