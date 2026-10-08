#include "Economy/ProductionChains.h"
#include "Simulation/BuildingLook.h"
#include "Simulation/IslandRegistry.h"
#include "Simulation/OccupancyGrid.h"
#include "Simulation/Placement.h"
#include "Simulation/ProducerLocation.h"
#include "Simulation/RoadNetwork.h"
#include "Simulation/TreeRegistry.h"
#include "TestWorld.h"
#include "World/BlockTypes.h"
#include "World/Chunk.h"
#include "World/FelledTrees.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

const ProductionChain& Fishery() { return PRODUCTION_CHAINS[BUILDING_TYPES[BUILDING_FISHERY].chain]; }
const ProductionChain& SheepFarm() { return PRODUCTION_CHAINS[BUILDING_TYPES[BUILDING_SHEEP_FARM].chain]; }
const ProductionChain& Lumberjack() { return PRODUCTION_CHAINS[BUILDING_TYPES[BUILDING_LUMBERJACK].chain]; }

const glm::ivec2 PRODUCER_TILES(3, 3);

glm::ivec2 SpawnTile() {
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    return glm::ivec2(ColumnToTile(spawn.x), ColumnToTile(spawn.y));
}

// A 3x3 footprint near the spawn that is far from the sea
glm::ivec2 InlandTile() {
    TestWorld& test = TestWorld::Get();
    for (int ring = 0; ring < 30; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                glm::ivec2 tile = SpawnTile() + glm::ivec2(dx, dz);
                if (!HasCoast(test.terrain, tile, PRODUCER_TILES, Fishery().radius + 2)) return tile;
            }
        }
    }
    return SpawnTile();
}

// The open sea nearest the spawn
glm::ivec2 SeaColumn() {
    TestWorld& test = TestWorld::Get();
    const glm::ivec2 directions[8] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    for (int ring = 1; ring < 200; ring++) {
        for (const glm::ivec2& direction : directions) {
            glm::ivec2 column = test.spawnColumn + direction * ring * 32;
            if (test.terrain.TerrainHeightAt(column.x, column.y) < SEA_LEVEL - 6) return column;
        }
    }
    return test.spawnColumn;
}

// A box of columns near the spawn that has trees in it
bool FindForest(TreeRegistry& trees, glm::ivec2& minColumn, glm::ivec2& maxColumn) {
    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    for (int ring = 0; ring < 20; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                glm::ivec2 corner = spawn + glm::ivec2(dx, dz) * 96;
                if (trees.CountStanding(corner, corner + 95) >= 2) {
                    minColumn = corner;
                    maxColumn = corner + 95;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST(ProducerLocationTest, CoastIsFoundNearTheSeaOnly) {
    TestWorld& test = TestWorld::Get();
    glm::ivec2 sea = SeaColumn();
    glm::ivec2 seaTile(ColumnToTile(sea.x), ColumnToTile(sea.y));
    EXPECT_TRUE(HasCoast(test.terrain, seaTile, PRODUCER_TILES, Fishery().radius));
    EXPECT_FALSE(HasCoast(test.terrain, InlandTile(), PRODUCER_TILES, Fishery().radius));
}

TEST(ProducerLocationTest, FisheryInlandIsRefused) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    RoadNetwork roads;
    PlacementContext context{ test.world, islands, occupancy, roads };
    // An inland spot where a farmer house (same footprint) fits
    for (int ring = 0; ring < 20; ring++) {
        for (int dz = -ring; dz <= ring; dz++) {
            for (int dx = -ring; dx <= ring; dx++) {
                glm::ivec2 tile = InlandTile() + glm::ivec2(dx, dz);
                if (HasCoast(test.terrain, tile, PRODUCER_TILES, Fishery().radius)) continue;
                if (ValidatePlacement(context, BUILDING_FARMER_HOUSE, 0, tile).error != PlacementError::None) continue;
                EXPECT_EQ(ValidatePlacement(context, BUILDING_FISHERY, 0, tile).error, PlacementError::DockNotOverWater);
                EXPECT_EQ(ValidatePlacement(context, BUILDING_SAWMILL, 0, tile).error, PlacementError::None); // No location rule
                return;
            }
        }
    }
    FAIL() << "No inland building spot found";
}

// A building object of a type at a tile, as BuildTool::Place sets it up
static GameObjectId AddBuilding(GameObjectRegistry& objects, uint16_t type, glm::ivec2 minTile, GameObjectId owner = INVALID_GAME_OBJECT) {
    GameObjectId id = objects.Create();
    objects.Building(id).type = type;
    objects.Building(id).island = 1;
    objects.Building(id).owner = owner;
    objects.Anchor(id).origin = glm::ivec3(minTile.x * TILE_SIZE, 0, minTile.y * TILE_SIZE);
    objects.Anchor(id).footprint = FootprintTiles(BUILDING_TYPES[type], 0) * TILE_SIZE;
    return id;
}

TEST(ProducerLocationTest, FarmModulesCountInRangeUpToTheFullCount) {
    GameObjectRegistry objects;
    const glm::ivec2 farmTile(100, 100);
    GameObjectId farm = AddBuilding(objects, BUILDING_SHEEP_FARM, farmTile);
    const glm::ivec2 fold = FootprintTiles(BUILDING_TYPES[BUILDING_SHEEPFOLD], 0);
    const int radius = SheepFarm().radius;

    // Within the radius on every side, not one tile beyond it
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 1, farmTile - radius, fold), farm);
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 1, farmTile + glm::ivec2(3 + radius) - fold, fold), farm);
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 1, farmTile - radius - 1, fold), INVALID_GAME_OBJECT);
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 2, farmTile - radius, fold), INVALID_GAME_OBJECT); // Other island
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_PIGSTY, 1, farmTile - radius, glm::ivec2(2, 3)), INVALID_GAME_OBJECT); // Not its farm

    // Each owned module counts; the farm is full at the chain's full-speed count
    for (int i = 0; i < SheepFarm().fullSpeedCount; i++) {
        EXPECT_EQ(CountModules(objects, farm), i);
        AddBuilding(objects, BUILDING_SHEEPFOLD, farmTile + glm::ivec2(-3, -3 + 3 * i), farm);
    }
    EXPECT_EQ(CountModules(objects, farm), SheepFarm().fullSpeedCount);
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 1, farmTile + glm::ivec2(3, 0), fold), INVALID_GAME_OBJECT);

    // A second farm nearby takes the next one; a module out of its farm's range stops counting
    GameObjectId second = AddBuilding(objects, BUILDING_SHEEP_FARM, farmTile + glm::ivec2(6, 0));
    EXPECT_EQ(FindModuleFarm(objects, BUILDING_SHEEPFOLD, 1, farmTile + glm::ivec2(3, 0), fold, farm), second);
    objects.Anchor(farm).origin.x -= 20 * TILE_SIZE; // The farm moved away
    EXPECT_EQ(CountModules(objects, farm), 0);
}

TEST(TreeRegistryTest, TreesStandApart) {
    TestWorld& test = TestWorld::Get();
    std::vector<glm::ivec2> roots;
    test.terrain.ForEachTreeRoot(test.spawnColumn - 200, test.spawnColumn + 200, [&](glm::ivec2 root) { roots.push_back(root); });
    ASSERT_FALSE(roots.empty());
    for (size_t i = 0; i < roots.size(); i++) {
        for (size_t j = i + 1; j < roots.size(); j++) {
            glm::ivec2 d = glm::abs(roots[i] - roots[j]);
            EXPECT_FALSE(d.x <= TerrainGenerator::TREE_SPACING && d.y <= TerrainGenerator::TREE_SPACING)
                << roots[i].x << "," << roots[i].y << " and " << roots[j].x << "," << roots[j].y;
        }
    }
}

TEST(TreeRegistryTest, LumberjackCountsTheTreesAroundIt) {
    TestWorld& test = TestWorld::Get();
    IslandRegistry islands(test.terrain);
    OccupancyGrid occupancy;
    RoadNetwork roads;
    TreeRegistry trees(test.terrain);
    glm::ivec2 minColumn, maxColumn;
    ASSERT_TRUE(FindForest(trees, minColumn, maxColumn));
    glm::ivec2 tile(ColumnToTile(minColumn.x + 48), ColumnToTile(minColumn.y + 48));
    int radius = Lumberjack().radius;
    int expected = trees.CountStanding((tile - radius) * TILE_SIZE, (tile + PRODUCER_TILES + radius) * TILE_SIZE - 1);
    LocationReport report = EvaluateLocation(Lumberjack(), tile, PRODUCER_TILES, islands, occupancy, roads, trees);
    EXPECT_EQ(report.count, expected);
    EXPECT_GT(report.count, 0);
}

TEST(TreeRegistryTest, FellingAndRegrowing) {
    TestWorld& test = TestWorld::Get();
    OccupancyGrid occupancy;
    RoadNetwork roads;
    TreeRegistry trees(test.terrain);
    glm::ivec2 minColumn, maxColumn;
    ASSERT_TRUE(FindForest(trees, minColumn, maxColumn));
    int standing = trees.CountStanding(minColumn, maxColumn);
    uint32_t revision = trees.Revision();

    ASSERT_TRUE(trees.FellNearest((minColumn + maxColumn) / 2, minColumn, maxColumn, 100));
    EXPECT_EQ(trees.CountStanding(minColumn, maxColumn), standing - 1);
    EXPECT_NE(trees.Revision(), revision);
    ASSERT_EQ(trees.Changes().size(), 1u);
    glm::ivec2 root = trees.Changes()[0].root;
    EXPECT_TRUE(trees.Changes()[0].felled);
    EXPECT_TRUE(trees.IsFelled(root));
    EXPECT_TRUE(trees.Felled().Contains(root));
    trees.ClearChanges();

    trees.Update(100 + TreeRegistry::REGROW_TICKS - 1, occupancy, roads);
    EXPECT_TRUE(trees.IsFelled(root));
    trees.Update(100 + TreeRegistry::REGROW_TICKS, occupancy, roads);
    EXPECT_FALSE(trees.IsFelled(root));
    EXPECT_FALSE(trees.Felled().Contains(root));
    ASSERT_EQ(trees.Changes().size(), 1u);
    EXPECT_FALSE(trees.Changes()[0].felled);
    EXPECT_EQ(trees.CountStanding(minColumn, maxColumn), standing);
}

TEST(TreeRegistryTest, NoRegrowthUnderABuilding) {
    TestWorld& test = TestWorld::Get();
    OccupancyGrid occupancy;
    RoadNetwork roads;
    TreeRegistry trees(test.terrain);
    glm::ivec2 minColumn, maxColumn;
    ASSERT_TRUE(FindForest(trees, minColumn, maxColumn));
    ASSERT_TRUE(trees.FellNearest(minColumn, minColumn, maxColumn, 0));
    glm::ivec2 root = trees.Changes()[0].root;
    glm::ivec2 rootTile(ColumnToTile(root.x), ColumnToTile(root.y));
    occupancy.Occupy(rootTile, glm::ivec2(1), 42);

    trees.Update(TreeRegistry::REGROW_TICKS, occupancy, roads);
    EXPECT_TRUE(trees.IsFelled(root)); // Waits while the building stands

    occupancy.Release(rootTile, glm::ivec2(1), 42);
    trees.Update(TreeRegistry::REGROW_TICKS + TreeRegistry::REGROW_RETRY_TICKS, occupancy, roads);
    EXPECT_FALSE(trees.IsFelled(root));
}

TEST(TreeRegistryTest, GeneratedChunksLeaveFelledTreesOut) {
    VoxModel model;
    ASSERT_TRUE(model.Load(std::string(ASSET_DIRECTORY) + "/tree.vox"));
    TerrainGenerator generator(model);
    FelledTrees felled;
    generator.SetFelledTrees(&felled);

    glm::ivec2 spawn = TestWorld::Get().spawnColumn;
    bool found = false;
    glm::ivec2 root(0);
    generator.ForEachTreeRoot(spawn - 300, spawn + 300, [&](glm::ivec2 candidate) {
        if (!found) {
            root = candidate;
            found = true;
        }
    });
    ASSERT_TRUE(found);

    int cy = (generator.TreeRootY(root.x, root.y) + 2) >> 5;
    auto solidVoxels = [](const std::vector<uint8_t>& data) { return std::count_if(data.begin(), data.end(), [](uint8_t id) { return id != 0; }); };
    std::vector<uint8_t> standing, cut;
    generator.GenerateChunk(root.x >> 5, cy, root.y >> 5, standing);
    felled.Add(root);
    generator.GenerateChunk(root.x >> 5, cy, root.y >> 5, cut);
    EXPECT_LT(solidVoxels(cut), solidVoxels(standing));
}
