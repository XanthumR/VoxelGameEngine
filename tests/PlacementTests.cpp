#include "Simulation/BuildingLook.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "Simulation/RoadNetwork.h"
#include "TestWorld.h"
#include "World/BlockTypes.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

namespace {

// Fresh islands, occupancy and roads on the shared generated world
struct PlacementFixture {
    IslandRegistry islands{ TestWorld::Get().terrain };
    OccupancyGrid occupancy;
    RoadNetwork roads;
    PlacementContext context{ TestWorld::Get().world, islands, occupancy, roads };
};

glm::ivec2 SpawnTile() {
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    return glm::ivec2(ColumnToTile(spawn.x), ColumnToTile(spawn.y));
}

// Nearest tile (searching outwards from spawn) where the building fits
bool FindValidTile(const PlacementContext& context, uint16_t type, glm::ivec2& tile) {
    for (int ring = 0; ring < 20; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                glm::ivec2 candidate = SpawnTile() + glm::ivec2(dx, dz);
                if (ValidatePlacement(context, type, 0, candidate).error == PlacementError::None) {
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
    PlacementFixture f;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(f.context, BUILDING_WAREHOUSE, tile));

    PlacementCheck check = ValidatePlacement(f.context, BUILDING_WAREHOUSE, 0, tile);
    EXPECT_EQ(check.error, PlacementError::None);
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    EXPECT_EQ(check.island, f.islands.IslandIdAt(spawn.x, spawn.y));
}

TEST(PlacementTest, OccupiedTilesAreRejected) {
    PlacementFixture f;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(f.context, BUILDING_WAREHOUSE, tile));

    f.occupancy.Occupy(tile, FootprintTiles(BUILDING_TYPES[BUILDING_WAREHOUSE], 0), 1);
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_WAREHOUSE, 0, tile).error, PlacementError::Occupied);
    // A house overlapping one corner tile
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile + glm::ivec2(3, 3)).error, PlacementError::Occupied);
}

TEST(PlacementTest, RoadUnderTheFootprintIsRejected) {
    PlacementFixture f;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(f.context, BUILDING_FARMER_HOUSE, tile));
    f.roads.Add(tile + glm::ivec2(2, 1));
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile).error, PlacementError::Road);
    // Right next to the footprint is fine
    f.roads.Remove(tile + glm::ivec2(2, 1));
    f.roads.Add(tile + glm::ivec2(3, 0));
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile).error, PlacementError::None);
}

TEST(PlacementTest, WaterIsRejected) {
    PlacementFixture f;
    TestWorld& test = TestWorld::Get();
    // The nearest open sea to the spawn (from the terrain noise), then generate the world there
    bool found = false;
    glm::ivec2 sea(0);
    const glm::ivec2 directions[8] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    for (int ring = 1; ring < 200 && !found; ring++) {
        for (const glm::ivec2& direction : directions) {
            if (found) break;
            glm::ivec2 column = test.spawnColumn + direction * ring * 32;
            if (test.terrain.TerrainHeightAt(column.x, column.y) < SEA_LEVEL - 6) {
                sea = column;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found) << "No sea near the spawn island";
    test.LoadAround(sea, 1);

    // A tile whose first column is that open water
    glm::ivec2 tile(ColumnToTile(sea.x), ColumnToTile(sea.y));
    glm::ivec2 column = tile * TILE_SIZE;
    ASSERT_EQ(test.world.GetVoxel(column.x, SEA_LEVEL, column.y), Block::WATER);
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile).error, PlacementError::Water);
    EXPECT_EQ(ValidateRoadTile(f.context, tile).error, PlacementError::Water);
}

TEST(PlacementTest, SomethingSolidInTheVolumeBlocksIt) {
    PlacementFixture f;
    VoxelWorld& world = TestWorld::Get().world;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(f.context, BUILDING_FARMER_HOUSE, tile));

    glm::ivec3 rock(tile.x * TILE_SIZE + 5, BUILD_GROUND_Y + 2, tile.y * TILE_SIZE + 5);
    ASSERT_TRUE(world.SetVoxel(rock.x, rock.y, rock.z, Block::STONE));
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile).error, PlacementError::Blocked);
    world.SetVoxel(rock.x, rock.y, rock.z, Block::AIR); // The world is shared by other tests
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, tile).error, PlacementError::None);
}

TEST(PlacementTest, UnloadedWorldIsRejected) {
    PlacementFixture f;
    glm::ivec2 farAway = SpawnTile() + glm::ivec2(1000, 0);
    EXPECT_EQ(ValidatePlacement(f.context, BUILDING_FARMER_HOUSE, 0, farAway).error, PlacementError::NotLoaded);
    EXPECT_EQ(ValidateRoadTile(f.context, farAway).error, PlacementError::NotLoaded);
}

TEST(PlacementTest, UnknownTypeIsRejected) {
    PlacementFixture f;
    EXPECT_NE(ValidatePlacement(f.context, (uint16_t)BUILDING_TYPES.size(), 0, SpawnTile()).error, PlacementError::None);
}

TEST(PlacementTest, RoadTileRules) {
    PlacementFixture f;
    glm::ivec2 tile;
    ASSERT_TRUE(FindValidTile(f.context, BUILDING_FARMER_HOUSE, tile));
    EXPECT_EQ(ValidateRoadTile(f.context, tile).error, PlacementError::None);

    f.roads.Add(tile);
    EXPECT_EQ(ValidateRoadTile(f.context, tile).error, PlacementError::Road); // Already road
    f.roads.Remove(tile);

    f.occupancy.Occupy(tile, glm::ivec2(1), 7);
    EXPECT_EQ(ValidateRoadTile(f.context, tile).error, PlacementError::Occupied); // A building stands there
}
